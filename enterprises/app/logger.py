import logging
from logging.handlers import RotatingFileHandler
import os
import sys
from app.config import get_config

config = get_config()


def setup_logger(name, log_file, level=logging.INFO):
    """Function to set up a logger that writes to both file and console"""
    # Create logs directory if it doesn't exist
    log_dir = os.path.dirname(log_file)
    os.makedirs(log_dir, exist_ok=True)

    # Create formatter
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s')

    # Create file handler
    file_handler = RotatingFileHandler(
        log_file, maxBytes=10*1024*1024, backupCount=5)
    file_handler.setFormatter(formatter)

    # Create console handler
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)

    # Get logger
    logger = logging.getLogger(name)
    logger.setLevel(level)

    # Add handlers
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)

    return logger, file_handler


# Create main application logger
server_logger = None
task_logger = None
file_handle_logger = None
file_handle_task_logger = None


def get_logger():
    global server_logger, file_handle_logger
    if server_logger is None:
        server_logger, file_handle_logger = setup_logger(
            'server', config.log_server_file_path)
    return server_logger


def get_task_logger():
    global task_logger, file_handle_task_logger
    if task_logger is None:
        task_logger, file_handle_task_logger = setup_logger(
            'task', config.log_task_file_path)
    return task_logger


get_logger()
get_task_logger()
# Configure SQLAlchemy logging
logging.getLogger('sqlalchemy.engine').setLevel(logging.INFO)
logging.getLogger('sqlalchemy.engine').addHandler(file_handle_logger)


def log_exception(exc_type, exc_value, exc_traceback):
    """Function to log uncaught exceptions"""
    if issubclass(exc_type, KeyboardInterrupt):
        # Call the default excepthook for KeyboardInterrupt
        sys.__excepthook__(exc_type, exc_value, exc_traceback)
        return

    get_logger().error("Uncaught exception: ", exc_info=(
        exc_type, exc_value, exc_traceback))


# Set the exception hook
sys.excepthook = log_exception


class StreamToLogger(object):
    def __init__(self, logger, log_level=logging.INFO):
        self.logger = logger
        self.log_level = log_level
        self.linebuf = ''
        self.terminal = sys.stdout

    def write(self, buf):
        for line in buf.rstrip().splitlines():
            self.logger.log(self.log_level, line.rstrip())
        self.terminal.write(buf)

    def flush(self):
        pass

    def isatty(self):
        return False

    def fileno(self):
        return self.terminal.fileno()


original_stdout = sys.stdout
original_stderr = sys.stderr


def redirect_stdout_stderr():
    sys.stdout = StreamToLogger(get_logger(), logging.INFO)
    sys.stderr = StreamToLogger(get_logger(), logging.ERROR)


def restore_stdout_stderr():
    sys.stdout = original_stdout
    sys.stderr = original_stderr
