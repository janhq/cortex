import yaml, os
from pathlib import Path


class Config:
    def __init__(self, config_path: str = "config.yml"):
        self.config_path = Path(config_path)
        self.default_config = {
            "database": {
                "url": "sqlite:///./sql_app.db"
            },
            "celery": {
                "broker_url": "redis://redis:6379/0",
                "result_backend": "redis://redis:6379/0",
                "task_routes": {}
            },
            "api": {
                "host": "0.0.0.0",
                "port": 8000
            },
            "redis": {
                "host": "redis",
                "port": 6379
            },
            "flower": {
                "port": 5555
            }
        }
        self.config = self._load_config()

    def _load_config(self):
        with open(self.config_path, "r") as config_file:
            try:
                raw_config = yaml.safe_load(config_file)
            except yaml.YAMLError:
                return self.default_config

        if raw_config is None:
            return self.default_config
        self.config = raw_config
        # Merge default config with raw config
        for section in self.default_config.keys():
            if section not in raw_config or isinstance(raw_config[section], dict):
                self.config[section] = {
                    **self.default_config[section], **(raw_config.get(section, {}))}

        return self.config
    @property
    def whisper_endpoint(self):
        return os.environ.get("WHISPER_ENDPOINT",self.config["whisper"]["endpoint"] )

    @property
    def database_url(self):
        return self.config["database"]["url"]

    @property
    def celery_broker_url(self):
        return self.config["celery"]["broker_url"]

    @property
    def celery_result_backend(self):
        return self.config["celery"]["result_backend"]

    @property
    def celery_result_expires(self):
        return self.config["celery"]["result_expires"]
    
    @property
    def celery_job_queue(self):
        return self.config["celery"]["job_queue"]

    @property
    def celery_task_routes(self):
        if isinstance(self.config["celery"], dict) and "task_routes" in self.config["celery"]:
            return self.config["celery"]["task_routes"]
        else:
            return {}
    @property
    def llm_endpoint(self):
        return os.environ.get("LLM_ENDPOINT",self.config["llm"]["endpoint"] )
    @property
    def tts_endpoint(self):
        return os.environ.get("TTS_ENDPOINT",self.config["tts"]["endpoint"] )
    @property
    def api_host(self):
        return self.config["api"]["host"]

    @property
    def api_port(self):
        return self.config["api"]["port"]

    @property
    def redis_host(self):
        return self.config["redis"]["host"]

    @property
    def redis_port(self):
        return self.config["redis"]["port"]

    @property
    def flower_port(self):
        return self.config["flower"]["port"]

    @property
    def secret_key(self):
        return self.config["jwt"]["secret_key"]

    @property
    def algorithm(self):
        return self.config["jwt"]["algorithm"]

    @property
    def access_token_expire_minutes(self):
        return self.config["jwt"]["access_token_expire_minutes"]

    @property
    def log_server_file_path(self):
        return self.config["logging"]["server_log_file"]

    @property
    def log_task_file_path(self):
        return self.config["logging"]["task_log_file"]


# Create a global instance of the Config class
config = None


def get_config():
    global config
    if config is None:
        config = Config()
    return config
