import torch
import torch.nn as nn
import whisper
from whisper.model import AudioEncoder, ModelDimensions
from typing import Dict, Optional
from whisperspeech.vq_stoks import RQBottleneckTransformer, Tunables
from huggingface_hub import hf_hub_download
import torch.nn.functional as F
import os
from typing import List, Optional, Union
import io
import urllib
from tqdm import tqdm
import torchaudio
_HF_MODELS = {  
    "medium": "https://huggingface.co/jan-hq/WhisperVQ/resolve/main/medium_encoder_only.pt",
}
def available_models() -> List[str]:
    """Returns the names of available models"""
    return list(_HF_MODELS.keys())
def _download(url: str, root: str, in_memory: bool) -> Union[bytes, str]:
    os.makedirs(root, exist_ok=True)

    expected_sha256 = url.split("/")[-2]
    download_target = os.path.join(root, os.path.basename(url))

    if os.path.exists(download_target) and not os.path.isfile(download_target):
        raise RuntimeError(f"{download_target} exists and is not a regular file")

    if os.path.isfile(download_target):
        with open(download_target, "rb") as f:
            model_bytes = f.read()
        return model_bytes if in_memory else download_target

    with urllib.request.urlopen(url) as source, open(download_target, "wb") as output:
        with tqdm(
            total=int(source.info().get("Content-Length")),
            ncols=80,
            unit="iB",
            unit_scale=True,
            unit_divisor=1024,
        ) as loop:
            while True:
                buffer = source.read(8192)
                if not buffer:
                    break

                output.write(buffer)
                loop.update(len(buffer))

    model_bytes = open(download_target, "rb").read()
    return model_bytes if in_memory else download_target
class CustomWhisperEncoder(nn.Module):
    """
    Lightweight wrapper that only loads the AudioEncoder part of Whisper
    """
    def __init__(self, name: str, device: str = None, download_root: str = None, in_memory: bool = False,):
        super().__init__()
        if device is None:
            device = "cuda" if torch.cuda.is_available() else "cpu"
        if download_root is None:
            default = os.path.join(os.path.expanduser("~"), ".cache")
            download_root = os.path.join(os.getenv("XDG_CACHE_HOME", default), "whisper")

        if name in _HF_MODELS:
            checkpoint_file = _download(_HF_MODELS[name], download_root, in_memory)
        elif os.path.isfile(name):
            checkpoint_file = open(name, "rb").read() if in_memory else name
        else:
            raise RuntimeError(
                f"Model {name} not found; available models = {available_models()}"
            )
        
        # Load weights
        with (
            io.BytesIO(checkpoint_file) if in_memory else open(checkpoint_file, "rb")
        ) as fp:
            checkpoint = torch.load(fp, map_location=device)
        del checkpoint_file
        dims = ModelDimensions(**checkpoint["dims"])
        self.encoder = AudioEncoder(
            dims.n_mels,
            dims.n_audio_ctx,
            dims.n_audio_state,
            dims.n_audio_head,
            dims.n_audio_layer,
        )
        
        self.encoder.load_state_dict(checkpoint["model_state_dict"])
        
        if device:
            self.to(device)
        
        self.eval()

    def forward(self, mel: torch.Tensor):
        return self.encoder(mel)
    
class CustomRQBottleneckTransformer(RQBottleneckTransformer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    @classmethod
    def load_vq_only(cls, ref="collabora/spear-tts-pytorch:whisper-vq-stoks-medium-en+pl.model",
                   repo_id=None, filename=None, local_filename=None):
        if repo_id is None and filename is None and local_filename is None:
            if ":" in ref:
                repo_id, filename = ref.split(":", 1)
            else:
                local_filename = ref
        if not local_filename:
            local_filename = hf_hub_download(repo_id=repo_id, filename=filename)
        
        # Load the spec
        spec = torch.load(local_filename)
        
        # Create instance with minimal required components
        instance = cls(**spec['config'], tunables=Tunables(**Tunables.upgrade(spec.get('tunables', {}))))
        
        # Load only necessary state dict entries
        required_components = {
            'rq', 'mlp', 'mlp_ln'
        }
        filtered_state_dict = {
            k: v for k, v in spec['state_dict'].items()
            if any(k.startswith(comp) for comp in required_components)
        }
        
        instance.load_state_dict(filtered_state_dict, strict=False)
        instance.eval()
        return instance

    def load_encoder(self, device=None):
        if self.whmodel is not None: return
        device = device or self.device
        # Use our custom encoder-only model
        if self.whmodel is None:
            encoder = CustomWhisperEncoder(self.whisper_model_name, device=device)
            self.whmodel = [encoder]
        multilingual = not self.whisper_model_name.endswith('.en')
        self.tokenizer = whisper.tokenizer.get_tokenizer(multilingual)

    def optimzed_encode_mel(self, mel):
        assert len(mel.shape) == 3, "invalid mel spectrogram shape, expect (batch,chn,time)"
        self.load_encoder()
        n = mel.shape[-1]
        if n > whisper.audio.N_FRAMES:
            padding = 0
            padded = mel[:,:,:whisper.audio.N_FRAMES]
        else:
            padding = -n % whisper.audio.N_FRAMES
            padded = F.pad(mel, (0, padding), value=-1.5)
        embs = self.whmodel[0].encoder(padded)#.to(self.whmodel[0].device))#[:,:n//2]
        stoks = self.quantize(embs)
        if self.tunables.mask_embs:
            return stoks[:,:n//2//self.downsample]
        else:
            return stoks
    # overide
    def encode_audio(self, audio):
        if isinstance(audio, str):
            x, sr = torchaudio.load(audio)
            x = torchaudio.transforms.Resample(sr, 16000)(x)[0]
            audio = x.unsqueeze(0)
        return self.optimzed_encode_mel(self.log_mel_spectrogram(audio).to(self.device))
    
if __name__ == "__main__":
    # Load the model
    vqmodel = CustomRQBottleneckTransformer.load_vq_only(
        "whisper-vq-stoks-v3-7lang-fixed.model"
    ).to("cuda")
    vqmodel.load_encoder('cuda')
    vqmodel.eval()