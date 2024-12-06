import unittest
import struct
import serial
import yubino.device
import hashlib
import ecdsa
import time
import secrets
import logging

DEVICE="/dev/ttyACM0"
BAUD_RATE=115200

class TestDevice(unittest.TestCase):

    def setUp(self):
        logging.disable(logging.CRITICAL)
        self.device = serial.Serial(port=DEVICE, baudrate=BAUD_RATE, exclusive=True)
        # Give the mcu some time to restart
        time.sleep(2)

    def test_reset(self):
        yubino.device.reset(self.device)
        entries = yubino.device.list_credentials(self.device)
        self.assertEqual(len(entries), 0)
        yubino.device.make_credential(self.device, "toto")
        yubino.device.make_credential(self.device, "tutu")
        yubino.device.reset(self.device)
        entries = yubino.device.list_credentials(self.device)
        self.assertEqual(len(entries), 0)

    def test_make_credentials(self):
        yubino.device.reset(self.device)
        (toto_id, toto_key) = yubino.device.make_credential(self.device, "toto")
        (tutu_id, tutu_key) = yubino.device.make_credential(self.device, "tutu")
        entries = yubino.device.list_credentials(self.device)
        self.assertEqual(len(entries), 2)
        expected = {
            hashlib.sha1("toto".encode()).digest(): toto_id,
            hashlib.sha1("tutu".encode()).digest(): tutu_id,
        }
        for entry in entries:
            self.assertTrue(entry['hashed_app_id'] in expected)
            self.assertEqual(expected[entry['hashed_app_id']], entry['credential_id'])

    def test_make_credentials_already_existing(self):
        yubino.device.reset(self.device)
        (first_id, _) = yubino.device.make_credential(self.device, "toto")
        (second_id, _) = yubino.device.make_credential(self.device, "toto")
        entries = yubino.device.list_credentials(self.device)
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]['hashed_app_id'], hashlib.sha1("toto".encode()).digest())
        self.assertEqual(entries[0]['credential_id'], second_id)

    def test_make_credentials_full(self):
        yubino.device.reset(self.device)
        with self.assertRaises(Exception) as ex:
            for i in range(100):
                yubino.device.make_credential(self.device, f"toto {i}")
        # 5 = STATUS_ERR_STORAGE_FULL
        self.assertEqual(ex.exception.args[0], "Device returned error code 5")

    def test_get_assertion_app_unknown(self):
        yubino.device.reset(self.device)
        challenge = secrets.token_hex(64)
        with self.assertRaises(Exception) as ex:
            yubino.device.get_assertion(self.device, "toto", challenge)
        # 4 = STATUS_ERR_NOT_FOUND
        self.assertEqual(ex.exception.args[0], "Device returned error code 4")

    def test_get_assertion(self):
        yubino.device.reset(self.device)
        (credential_id, public_key) = yubino.device.make_credential(self.device, "toto")
        challenge = secrets.token_hex(64)
        (used_credential_id, signature) = yubino.device.get_assertion(self.device, "toto", challenge)

        self.assertEqual(credential_id, used_credential_id)
        ecdsa_public_key = ecdsa.VerifyingKey.from_string(
                public_key,
                curve=ecdsa.SECP160r1)
        fixed_sig = b'\x00' + signature[:20] + b'\x00' + signature[20:]
        ecdsa_public_key.verify_digest(fixed_sig, yubino.device.get_client_data_hash(challenge, "toto"))

    def test_get_assertion_multiple_creds(self):
        yubino.device.reset(self.device)
        (toto_credential_id, toto_public_key) = yubino.device.make_credential(self.device, "toto")
        (tutu_credential_id, tutu_public_key) = yubino.device.make_credential(self.device, "tutu")

        challenge = secrets.token_hex(64)
        (used_credential_id, signature) = yubino.device.get_assertion(self.device, "toto", challenge)

        self.assertEqual(toto_credential_id, used_credential_id)
        ecdsa_public_key = ecdsa.VerifyingKey.from_string(
                toto_public_key,
                curve=ecdsa.SECP160r1)
        fixed_sig = b'\x00' + signature[:20] + b'\x00' + signature[20:]
        ecdsa_public_key.verify_digest(fixed_sig, yubino.device.get_client_data_hash(challenge, "toto"))

    def test_bad_command(self):
        self.device.write(struct.pack('B', 100))
        self.device.flush()

        status = struct.unpack('B', self.device.read())[0]
        # 1 = STATUS_ERR_COMMAND_UNKNOWN
        self.assertEqual(status, 1)



