import logging
import os
import serial.tools.list_ports
import cmd
import shlex
import secrets

import yubino.device
import yubino.web

class YubinoShell(cmd.Cmd):
    intro = 'Welcome to the Yubino shell. Type help or ? to list commands.\n'
    prompt = "yubino > "

    def __init__(self, config):
        super().__init__()
        self.config = config

    def preloop(self):
        logging.info("Connect to device %s", self.config.device)
        self.device = serial.Serial(port=self.config.device, baudrate=self.config.baud, exclusive=True)
        self.http_client = yubino.web.Client(self.config, self.device)

    def do_index(self, arg):
        """
        Fetch the index page
        """
        res = self.http_client.index()
        if res:
            print(res)

    def do_login(self, arg):
        """
        Login with username <user> on the relying party
        login <user>
        """
        if not arg:
            print("Usage: login <user>")
            return
        if self.http_client.login(arg):
            print("done")
        else:
            print("failed")

    def do_register(self, arg):
        """
        Register username <user> on the relying party
        register <user>
        """
        if not arg:
            print("Usage: register <user>")
            return
        if self.http_client.register(arg):
            print("done")
        else:
            print("failed")

    def do_logout(self, arg):
        """
        Reset HTTP client
        """
        self.http_client.reset()
        print("done")

    def do_device_reset(self, arg):
        'Reset the device'
        yubino.device.reset(self.device)

    def do_device_make_credential(self, arg):
        """
        Ask the device to generate a new keys for <app_id> pair
        and retrieve (credential_id, public_key).
        device_make_credential <app_id>
        """
        if not arg:
            print("Usage: device_make_credential <app_id>")
            return

        try:
            (credential_id, public_key) = yubino.device.make_credential(self.device, arg)
            print("Credential id: %s" %  credential_id.hex())
            print("Public key: %s" % public_key.hex())
        except Exception as e:
            print("Operation failed: %s" % e)

    def do_device_get_assertion(self, arg):
        """
        Ask the device to make an assertion on <challenge> for <app_id>.
        If <challenge> is not specified, it will be generated (32 bytes random).
        device_get_assertion <app_id> <challenge>
        """
        args = shlex.split(arg)
        if len(args) > 2 or len(args) == 0:
            print("Usage: get_assertion <app_id> <challenge>")
            return

        app_id = args[0]
        challenge = args[1] if len(args) == 2 else secrets.token_hex(32)

        try:
            (credential_id, signature) = yubino.device.get_assertion(self.device, app_id, challenge)
            print("credential_id: %s - signature: %s" % (credential_id.hex(), signature.hex()))
        except Exception as e:
            print("Operation failed: %s" % e)


    def do_device_list_credentials(self, arg):
        """
        List the credentials of the device
        """
        try:
            for entry in yubino.device.list_credentials(self.device):
                print("hashed_app_id: %s - credential_id: %s" % (entry['hashed_app_id'].hex(), entry['credential_id'].hex()))
        except Exception as e:
            print("Operation failed: %s" % e)


    def do_EOF(self, line):
        """
        Exit the shell
        """
        return True

