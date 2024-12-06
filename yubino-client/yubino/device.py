import struct
import hashlib
import logging

COMMAND_LIST_CREDENTIALS = 0
COMMAND_MAKE_CREDENTIAL = 1
COMMAND_GET_ASSERTION = 2
COMMAND_RESET = 3

STATUS_OK = 0
STATUS_ERR_COMMAND_UNKNOWN = 1

CREDENTIAL_ID_SIZE = 16
PUBLIC_KEY_SIZE = 40
APP_ID_SIZE = 20
SIGNATURE_SIZE = 40

def reset(device):
    """
    Send a RESET command to the device

    :return True if reset was succesful, False otherwise
    """
    logging.info("Sending RESET command")
    device.write(struct.pack('B', COMMAND_RESET))
    device.flush()

    status = struct.unpack('B', device.read())[0]

    logging.debug("Received status code %d", status)
    if status != STATUS_OK:
        logging.error("Something bad happened: error code %d", status)
        return False

    return True

def make_credential(device, app_id):
    """
    Send a MAKE_CREDENTIAL command to the device

    <app_id> is meant to be the "raw" app_id given by the Relying Party.
    It will be hashed in this function before being sent to the device.

    :except Exception: if the device returns an error.

    :return (<credential_id: bytes>, <public_key: bytes>), where
    - <credential_id> is the generated keypair identifier returned by the device
    - <public_key> is the publkic part of the generated key pair
    """
    hashed_app_id = hashlib.sha1(app_id.encode()).digest()
    logging.info("Sending MAKE_CREDENTIAL command with hashed_app_id=%s", hashed_app_id.hex())
    device.write(struct.pack('B', COMMAND_MAKE_CREDENTIAL))
    device.write(hashed_app_id)
    device.flush()

    status = struct.unpack('B', device.read())[0]
    logging.debug("Received status code %d", status)
    if status != STATUS_OK:
        logging.error("Something bad happened: error code %d", status)
        raise Exception(f"Device returned error code {status}")

    logging.debug("Retrieve credential_id")
    credential_id = device.read(CREDENTIAL_ID_SIZE)
    logging.debug("credential_id = %s", credential_id.hex())

    logging.debug("Retrieve public_key")
    public_key = device.read(PUBLIC_KEY_SIZE)
    logging.debug("public_key = %s", public_key.hex())

    return (credential_id, public_key)

def list_credentials(device):
    """
    Send a LIST_CREDENTIALS command to the device

    :except Exception: if the device returns an error

    :return List[{'hashed_app_id': <hashed_app_id>, 'credential_id': <credential_id>}, ...], where
    - <hashed_app_id> is the hashed app_id stored by the device
    - <credential_id> is the the key pair identifier corresponding to the relying party
    """
    logging.info("Sending LIST_CREDENTIALS command")
    device.write(struct.pack('B', COMMAND_LIST_CREDENTIALS))
    device.flush()

    status = struct.unpack('B', device.read())[0]
    logging.debug("Received status code %d", status)
    if status != STATUS_OK:
        logging.error("Something bad happened: error code %d", status)
        raise Exception(f"Device returned error code {status}")

    logging.debug("Retrieve credentials count")
    count = struct.unpack('B', device.read())[0]
    logging.debug("%d entries to retrieve", count)

    entries = []
    for i in range(count):
        logging.debug("Retrieve credential %d", i)
        credential_id = device.read(CREDENTIAL_ID_SIZE)
        logging.debug("credential_id = %s", credential_id.hex())

        app_id = device.read(APP_ID_SIZE)
        logging.debug("hashed_app_id = %s", app_id.hex())

        entries.append({'hashed_app_id': app_id, 'credential_id': credential_id})
    return entries


def get_client_data_hash(challenge, app_id):
    return hashlib.sha1(("challenge=%s&app_id=%s" % (challenge, app_id)).encode()).digest()

def get_assertion(device, app_id, challenge):
    """
    Send a GET_ASSERTION command to the device

    :param <app_id>: raw app_id given by the Relying Party
    :param <challenge>: raw challenge sent by the Relying Party. Must be a valid hexadecimal string.

    :except Exception: if the device returns an error

    :return (<credential_id: bytes>, <signature: bytes>) where
    - <credential_id> is the identifier of the key pair used to compute the signature
    - <signature> is the signature of clientDataHash
    """
    hashed_app_id = hashlib.sha1(app_id.encode()).digest()
    logging.info("Sending GET_ASSERTION command with hashed_app_id=%s and challenge=%s",
                 hashed_app_id.hex(), challenge)
    try:
        challenge_bytes = bytes.fromhex(challenge)
    except Exception as e:
        logging.error("Failed to convert challenge to bytes: %s", e)
        raise

    client_data_hash = get_client_data_hash(challenge, app_id)
    logging.debug("client_data_hash = %s", client_data_hash.hex())
    device.write(struct.pack('B', COMMAND_GET_ASSERTION))
    device.write(hashed_app_id)
    device.write(client_data_hash)
    device.flush()

    status = struct.unpack('B', device.read())[0]
    logging.debug("Received status code %d", status)
    if status != STATUS_OK:
        logging.error("Something bad happenned: error code %d", status)
        raise Exception(f"Device returned error code {status}")

    logging.debug("Retrieve credential_id")
    credential_id = device.read(CREDENTIAL_ID_SIZE)
    logging.debug("credential_id = %s", credential_id.hex())

    logging.debug("Retrieve signature")
    signature = device.read(SIGNATURE_SIZE)
    logging.debug("signature = %s", signature.hex())

    return (credential_id, signature)
