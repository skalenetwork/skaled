import logging
import json
import os
import semver
import docker
from docker import APIClient

import os, sys, logging
from logging import Formatter, StreamHandler

handlers = []
formatter = Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')

stream_handler = StreamHandler(sys.stderr)
stream_handler.setFormatter(formatter)
stream_handler.setLevel(logging.INFO)
handlers.append(stream_handler)

logging.basicConfig(level=logging.DEBUG, handlers=handlers)


CONFIG_FILENAME = 'config.json'
HERE = os.path.dirname(os.path.realpath(__file__))
config_path = os.path.join(HERE, CONFIG_FILENAME)
dockerfile_path = HERE

DOCKER_USERNAME = os.environ.get('DOCKER_USERNAME')
DOCKER_PASSWORD = os.environ.get('DOCKER_PASSWORD')
BUMP_VERSION = os.environ['BUMP_VERSION']

with open(config_path, encoding='utf-8') as data_file:
    config = json.loads(data_file.read())

docker_client = docker.from_env()
cli = APIClient()

logger = logging.getLogger(__name__)

if DOCKER_USERNAME and DOCKER_PASSWORD:
    docker_client.login(username=DOCKER_USERNAME, password=DOCKER_PASSWORD)


def bump_version(increment):
    return getattr(semver, f'bump_{increment}')(config['version'])

def save_new_config(path, config):
    with open(path, 'w') as data_file:
        json.dump(config, data_file)

def build_image():
    logger.info(f'Building container: {config["name"]}')
    new_version = bump_version(BUMP_VERSION)

    logger.info(f'Bumping {config["name"]} container version: {config["version"]} => {new_version}')

    config['version'] = new_version
    save_new_config(config_path, config)

    tag = f'{config["name"]}:{new_version}'
    logger.info(f'Building image: {tag}')
    for line in cli.build(path=dockerfile_path, tag=tag):
        logger.info(line)
    logger.info(f'Build done: {tag}')

def push_image():
    fullname = construct_image_fullname(config['name'], config['version'])
    logger.info(f'Pushing {fullname}')
    for line in cli.push(config['name'], tag=config['version'], stream=True):
        logger.info(line)
    logger.info(f'Push done: {fullname}')

def construct_image_fullname(name, version):
    return f"{name}:{version}"


if __name__ == "__main__":
    build_image()
    push_image()
