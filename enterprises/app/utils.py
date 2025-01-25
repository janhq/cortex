import base64
def encode_audio_to_base64(byte_data: bytes) -> str:

    try:
        base64_encoded = base64.b64encode(byte_data).decode('utf-8')
        return base64_encoded
    except IOError as e:
        raise IOError(f"Error reading audio file: {e}")


def decode_base64_to_audio(
    base64_string: str
) -> bytes:
    """
    Decode a base64 string to audio bytes and optionally save to file.

    Args:
        base64_string (str): Base64 encoded string
        output_path (Optional[Union[str, Path]]): Path to save the decoded audio file

    Returns:
        bytes: Decoded audio bytes

    Raises:
        ValueError: If the base64 string is invalid
        IOError: If there's an error writing the file
    """
    try:
        audio_bytes = base64.b64decode(base64_string)
        return audio_bytes
    except base64.binascii.Error as e:
        raise ValueError(f"Invalid base64 string: {e}")
    except IOError as e:
        raise IOError(f"Error writing audio file: {e}")


def get_audio_content_by_id(id):
    return "<|sound_token|>"