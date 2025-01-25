import os
import asyncio
from celery import Celery


from app.config import get_config
# Configure Celery

__celery = None


def get_celery() -> Celery:
    """
    Get the current Celery instance.
    Initialize the instance if not yet done

    :return: The current Celery instance
    :rtype: Celery
    """

    global __celery
    if __celery is None:

        __celery = Celery(
            broker=get_config().celery_broker_url,
            backend=get_config().celery_result_backend,
            result_expires=get_config().celery_result_expires,
        )
        __celery.conf.update(
            {
                "accept_content": ["json", "msgpack"],
                "task_serializer": "msgpack",
                "task_compression": "zstd",
                "task_annotations": {'*': {'rate_limit': '1000/m'}}
            }
        )
        __celery.conf.task_routes = {
            "app.workers.tasks.*": {"queue": "jop_queue"}
        }

    return __celery

# Configure logging


async def a_get_result(result, poll_interval=0.05):
    while True:
        if result.ready():
            return result.get()
        await asyncio.sleep(poll_interval)
