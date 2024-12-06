# Client Yubino

Ce client permet de dialoguer via une interface série avec un _Authenticator_ implémentant le protocole CATP simplifié "Yubino". Il communique également avec un _Relying Party_ via un protocole non spécifié (sur HTTP(S)).

## Installation

Il est conseillé d'installer ce client dans un environnement Python virtuel.

- Créer un environnement virtuel dans le répertoire `.env` :

```bash
$ python3 -m venv .env
```

- Se placer dans l'environnement vituel :

```bash
$ source .env/bin/activate
```

- Installer le client et ses dépendances :

```bash
$ pip install .
```

- Vérifier l'installation en lançant la commande `yubino -h`:

```bash
$ yubino -h
usage: yubino [-h] [-d DEVICE] [-b BAUD] [-r RELYING_PARTY] [--list-devices] [-v]

options:
  -h, --help            show this help message and exit
  -d DEVICE, --device DEVICE
                        Connect to the given device, defaults to '/dev/ttyACM0'
  -b BAUD, --baud BAUD  Baud rate, defaults to 115200
  -r RELYING_PARTY, --relying-party RELYING_PARTY
                        Relying party to connect to, defaults to 'http://localhost:8000'
  --list-devices        List available serial devices
  -v, --verbose         Verbose mode
```

Pour sortir de l'environnement virtuel, exécuter la fonction `deactivate`.

## Utilisation

```
usage: yubino [-h] [-d DEVICE] [-b BAUD] [-r RELYING_PARTY] [--list-devices] [-v]

options:
  -h, --help            show this help message and exit
  -d DEVICE, --device DEVICE
                        Connect to the given device, defaults to '/dev/ttyACM0'
  -b BAUD, --baud BAUD  Baud rate, defaults to 115200
  -r RELYING_PARTY, --relying-party RELYING_PARTY
                        Relying party to connect to, defaults to 'http://localhost:8000'
  --list-devices        List available serial devices
  -v, --verbose         Verbose mode
```

Une carte Arduino connectée à l'ordinateur est nécessaire pour lancer le client. Le _path_ du _device_ exposant la liaison série avec la carte peut être précisé avec l'option `--device`, et vaut par défaut `/dev/ttyACM0`. En cas de doute, il est possible d'appeler le client avec l'option `--list-devices` pour lister les interfaces séries disponibles. Le baud rate à utiliser peut être spécifié avec l'option `--baud` et vaut par défaut `115 200`.

Le client peut se connecter à un _Relying Party_, dont on spécifiera l'URL complète via l'option `--relying-party`.

Il est possible d'augmenter le niveau de verbosité du client en utilisant l'option `-v` (très utile pour debugger).

Le client prend la forme d'un shell interractif dans lequel il est possible d'exécuter des commandes :

```bash
$ yubino
INFO:root:Connect to device /dev/ttyACM0
Welcome to the Yubino shell. Type help or ? to list commands.

yubino >
```

## Commandes

Les commandes disponibles sont séparées en deux catégories :

- celles qui sont préfixées par `device_` permettent d'interragir directement avec l'_Authenticator_. Elles peuvent être utiles pour faire du debug.
- les autres permettent d'interragir avec le _Relying Party_ en utilisant l'_Authenticator_.

### Commandes `device_*`

#### `device_reset`

Envoie la commande `RESET` à l'_Authenticator_, provoquant la suppression de toutes les clés enregistrées.

```
yubino > device_reset
INFO:root:Sending RESET command
```

#### `device_make_credential <app_id>`

Envoie la commande `MAKE_CREDENTIAL` à l'_Authenticator_, provoquant la génération d'une nouvelle paire de clés liée à l'empreinte de `<app_id>`. L'_Authenticator_ renvoie l'identifiant unique de la paire ainsi que la partie publique, qui sont tous deux affichés à l'utilisateur.

```
yubino > device_make_credential babar
INFO:root:Sending MAKE_CREDENTIAL command with hashed_app_id=e407245674a75c4bf77d51c25466ca005f6c7c46
Credential id: e5c6a20231dbb1afabe42877db590507
Public key: 06b3fd520117b392d512d67bc943581afb6cb738f1433ddf4b4cdadec1bb48caa7a2c786a590b383
```

#### `device_get_assertion <app_id> <challenge>`

Envoie la commande `GET_ASSERTION` à l'_Authenticator_, lui demandant d'effectuer la signature du `clientDataHash` (calculé par le client à partir de `app_id` et `challenge`). Le client récupère l'identifiant de la clée utilisée pour signer ainsi que la signature.

Attention : `challenge` doit être une chaîne hexadécimale (de longueur arbitraire).

```
yubino > device_get_assertion babar 5bd775703e109e1f0f58baa2fe580172413d974d
INFO:root:Sending GET_ASSERTION command with hashed_app_id=e407245674a75c4bf77d51c25466ca005f6c7c46 and challenge=5bd775703e109e1f0f58baa2fe580172413d974d
credential_id: e5c6a20231dbb1afabe42877db590507 - signature: 687f115c30bd2093fc923f129b643932dbb04f9a9c0469404bd8fb1ba6bf0c44052806f43dba1b1a
```

#### `device_list_credentials`

Envoie la commande `LIST_CREDENTIALS` à l'_Authenticator_, récupérant ainsi la liste des couples `(hashed_app_id, credential_id)` qu'il contient.

Remarque : une clé privée ne sortira jamais de l'_Authenticator_.

```
yubino > device_list_credentials
INFO:root:Sending LIST_CREDENTIALS command
hashed_app_id: e407245674a75c4bf77d51c25466ca005f6c7c46 - credential_id: e5c6a20231dbb1afabe42877db590507
```

### Commandes d'interraction avec le _Relying Party_

#### `index`

Permet d'afficher le contenu de la page d'accueil du _Relying Party_. Ce contenu change selon que l'utilisateur soit authentifié ou non.

Version "anonyme":

```
yubino > index
Salut utilisateur anonyme!
Créé un compte puis authentifie toi!
```

Version "authentifiée":

```
yubino > index
Bien ouej <name>, tu as réussi à t'authentifier :-)
```

#### `register <name>`

Enregistre auprès du _Relying Party_ un utilisateur avec le nom `<name>`. Si le nom est déjà pris, une erreur est renvoyée. Lors de l'enregistrement, le client demande à l'_Authenticator_ de générer une paire de clés pour le _Relying Party_ et lui transmet la clé publique ainsi que l'identifiant de la paire générée.

```
yubino > register super_utilisateur
INFO:root:Sending MAKE_CREDENTIAL command with hashed_app_id=334389048b872a533002b34d73f8c29fd09efc50
done
```

#### `login <name>`

Envoie une demande d'authentification au _Relying Party_ au nom de l'utilisateur `<name>`. Un `challenge` est retourné par le _Relying Party_ (même si l'utilisateur n'existe pas). Le client transmet ce challenge ainsi que l'identifiant du _Relying Party_ à l'_Authenticator_ afin d'obtenir une signature et un identifiant de clé, qu'il retransmet au _Relying Party_ pour validation. Si l'utilisateur existe, et si la clé utilisée correspond à celle enregistrée par le _Relying Party_ pour l'utilisateur, et si l'utilisateur prouve qu'il possède la clé privée associée (via la signature), alors l'authentification réussie.

```
yubino > login super_utilisateur
INFO:root:Sending GET_ASSERTION command with hashed_app_id=334389048b872a533002b34d73f8c29fd09efc50 and challenge=42b8dd6abd983ef704206d2aff61285063443d543046fe70a83bc10372e31b56142d85722aa8b5ac866c5db4d8f7f3c0fc7ffe2464e9e865f723110203fa97ac
done
```

En cas de réussite, une session d'authentification est créée.

#### `logout`

Détruit la session d'authentification courante.

## Exemple d'utilisation

```
$ yubino
INFO:root:Connect to device /dev/ttyACM0
Welcome to the Yubino shell. Type help or ? to list commands.

yubino > index
Salut utilisateur anonyme!
Créé un compte puis authentifie toi!
yubino > register super_utilisateur
INFO:root:Sending MAKE_CREDENTIAL command with hashed_app_id=334389048b872a533002b34d73f8c29fd09efc50
done
yubino > login super_utilisateur
INFO:root:Sending GET_ASSERTION command with hashed_app_id=334389048b872a533002b34d73f8c29fd09efc50 and challenge=42b8dd6abd983ef704206d2aff61285063443d543046fe70a83bc10372e31b56142d85722aa8b5ac866c5db4d8f7f3c0fc7ffe2464e9e865f723110203fa97ac
done
yubino > index
Bien ouej super_utilisateur, tu as réussi à t'authentifier :-)
yubino > logout
```

## Tests

Le client contient un ensemble de tests vérifiant le comportement de l'_Authenticator_.

Les tests peuvent être exécutés de la façon suivante :

```
$ python -m unittest tests.device -v
test_bad_command (tests.device.TestDevice.test_bad_command) ... ok
test_get_assertion (tests.device.TestDevice.test_get_assertion) ... ok
test_get_assertion_app_unknown (tests.device.TestDevice.test_get_assertion_app_unknown) ... ok
test_get_assertion_multiple_creds (tests.device.TestDevice.test_get_assertion_multiple_creds) ... ok
test_make_credentials (tests.device.TestDevice.test_make_credentials) ... ok
test_make_credentials_already_existing (tests.device.TestDevice.test_make_credentials_already_existing) ... ok
test_make_credentials_full (tests.device.TestDevice.test_make_credentials_full) ... ok
test_reset (tests.device.TestDevice.test_reset) ... ok

----------------------------------------------------------------------
Ran 8 tests in 55.714s

OK
```

Remarque : il est conseillé d'ajouter une option de compilation à l'_Authenticator_ afin de pouvoir désactiver la demande de consentement de l'utilisateur et lancer les tests sans interraction humaine.
