import requests
import logging
from urllib.parse import urlparse

import yubino.device

class Client:

    def __init__(self, config, device):
        self.config = config
        self.device = device
        self.host = self.config.relying_party
        parse_result = urlparse(self.host)
        self.hostname = parse_result.hostname
        if self.hostname is None:
            raise ValueError(f"Invalid relying party address: {self.host}")

        logging.debug("Hostname is set to %s", self.hostname)
        self.session = requests.Session()

    def index(self):
        r = self.session.get(f"{self.host}/")
        if r.status_code == requests.codes.okay:
            return r.json()['message']
        else:
            logging.error("%d: %s", r.status_code, r.text)
            return False

    def register(self, username):
        r = self.session.get(f"{self.host}/info")
        if r.status_code != requests.codes.okay:
            logging.error("Failed to retrieve app_id. Server returned %d", r.status_code)
            return False

        app_id = r.json()['app_id']

        if app_id != self.hostname:
            logging.error("app_id must be equal to hostname: %s != %s", app_id, self.hostname)
            return False

        try:
            credential_id, public_key = yubino.device.make_credential(self.device, app_id)
        except Exception as e:
            logging.error("Failed to generate a new credential: %s", e)
            return False

        r = self.session.post(
                f"{self.host}/register",
                json={
                    'name': username,
                    'credential_id': credential_id.hex(),
                    'public_key': public_key.hex()
                })

        if r.status_code != requests.codes.okay:
            logging.error("%d: %s", r.status_code, r.text)
        return True

    def login(self, username):
        r = self.session.post(
                f"{self.host}/challenge",
                json={'username': username})

        if r.status_code != requests.codes.okay:
            logging.error("Failed to retrieve challenge for user %s. Server returned %d", username, r.status_code)
            return False

        data = r.json()
        logging.debug("Challenge is %s", data["challenge"])

        try:
            credential_id, signature = yubino.device.get_assertion(self.device, data['app_id'], data['challenge'])
        except Exception as e:
            logging.error("Failed to get assertion: %s", e)
            return False

        r = self.session.post(
                f"{self.host}/login",
                json={
                    'name': username,
                    'credential_id': credential_id.hex(),
                    'signature': signature.hex()
                })

        if r.status_code != requests.codes.okay:
            logging.error("%d: %s", r.status_code, r.text)
            return False

        return True

    def reset(self):
        self.session = requests.Session()

