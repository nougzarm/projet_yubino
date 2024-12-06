Projet Yubino :
Dans ce projet nous allons concevoir un programme pour notre Arduino permettant de l'utiliser en tant qu'un Authenticator.
(voir sujet du projet)

Matériel utilisé :
    - Arduino
    - 1 Bouton
    - 1 Led
    - Résistance(s)
    - 4 Cables

Schéma éléctrique : fichier 'schema.png'

Programme "compilable" : dossier 'programme' -> make && make upload

Programme "à consulter" : fichier 'main.c' dans le dossier programme

---------------------------------------
Etapes dans la réalisation du projet :

1.  Réalisation du programme debounce permettant de 'capter' un appuie du bouton (TP5, partie 5).
    Test du bon fonctionnement du bouton via une fonction faisant intervenir la led. 
    -> Voir programme dans dossier 'etapes/1. Test du bouton'.

2.  Fonction de confirmation de l'utilisateur : Avant toute chose, nous avons besoin d'implémenter une fonction
    uint8_t demande_confirmation(); qui fait clignoter la led pendant 10 secondes et si l'utilisateur appuye sur le bouton durant ces
    10 secondes alors demande_confirmation() renvoie 1 et sinon alors renvoie 0. 
    Cette fonction sert afin de savoir si on va plus loin ou pas dans l'execution des fonctions principales.
    -> Voir programme dans dossier 'etapes/1. Test du bouton'.

3.  Configuration du périphérique UART. Nous avons commencé par le faire comme dans le TP4, cependant nous avons pu observer un comportement
    étrange dans la transmission des données. Ce comportement est dû à un mauvais calcul de UBRR dû à un problème d'arrondi avec
    115200 comme baudrate. Ainsi nous avons décidé d'utiliser la librairie setbaud.h pour un calcul plus précis du UBRR. Ce qui a 
    réglé le problème.

4.  Une fois que la communication marchait, nous avons intégré la librairie eeprom.h qui permet la gestion de la mémoire EEPROM.
    Via les macros EEMEM nous pouvons utiliser la mémoire EEPROM et y stocker des informations. Nous l'avons utilisé deux fois :
    voir partie 5 du code principal. En effet on a défini une liste donnees_eeprom[1000] et un octet compteur_eeprom qui désigne le nombre
    de clé cryptographiques dans la mémoire.
    
    Les clés cryptographiques sont stockées dans la liste donnees_eeprom[1000], les unes après les autres de la manière suivante :

            [app_id ~ 20 octets],[credential_id ~ 16 octets],[private_key ~ 21 octets],[flag ~ 1 octet]  =  58 octets

    Ainsi une clé cryptographique (avec les données qui lui sont associées) occupe 58 octets -> donc 58 cases de la liste donnees_eeprom[1000]
    Ce qui nous permet de stocker au plus 17 clés.

    Remarque : ici l'octet 'flag' désigne si la place est occupée ou non. Si il est égale à 0 alors la place est vide et si il vaut
    255 alors une clé est stocké à cette emplacement. Nous avons dû faire un certain nombre de choix sur la façon de stocker les clés
    et de leur suppression notamment. Par exemple, lors de la requete 'device_reset', nous nous contentons de mettre simplement les flags à
    0 et de ne pas réellement supprimer la trace des clés dans la mémoire. Ce choix peut être discuté. La mémoire EEPROM a un nombre de
    cycle de vie donné, ainsi nous 'préservons' la mémoire. Mais nous sommes conscients que cela est beaucoup moins sécurisé.

    Remarque 2 : L'octet de flag n'est pas nécessaire dans notre cas. Mais pourrait devenir utile si le stockage des clé se fait autrement.
    Par exemple si on décide d'enregistrer une nouvelle clé aléatoirement parmi les emplacements disponibles contrairement à ce qu'on fait
    actuellement et qui consiste à mettre la nouvelle clé à la suite. Ainsi nous pourrions utiliser la mémoire de manière uniforme et 
    donc en rejoignant la discussion qui suit, cela pourrait éviter que ce soit les premières cases de la mémoires qui soient le plus utilisées.

    Nous avons 58*17 = 986, donc par souci d'optimisation (de mémoire), nous avons décidé de définir donnees_eeprom[986] au lieu de donnees_eeprom[1000]
    car de toute les manières les dernieres cases seraient perdues.

    Puis pour stocker ou lire des données dans cette mémoire EEPROM, nous avons utilisé les fonctions 
        -   eeprom_write_byte()
        -   eeprom_read_byte()
        -   eeprom_write_block()
        -   eeprom_read_block()
    de la librairie eeprom.h.

5.  Une fois qu'on peut gérer la mémoire EEPROM, avant d'implémenter les fonctions 
        -   make_credential()
        -   list_credentials()
        -   get_assertion()
        -   command_reset()
    Il nous reste juste à pouvoir maîtriser la librairie uECC afin de pouvoir générer des paires de clés ainsi que de signer
    des messages (hachés). Pour cela nous importons la librairie dans le dossier courant.

    Pour pouvoir utiliser les fonctions de cette librairie nous avons eu besoin de définir une fonction (pseudo-)aléatoire sur la quelle
    tout se repose. Nous avons défini une fonction pseudo-aléatoire se basant sur rand() de la librairie <stdlib.h>.
    Cependant, nous devrions la remplacer par la fonction RAND_bytes(unsigned char *buf, int num); de la librairie OpenSSL car 
    cette dernière est cryptographiquement sûre. 
    
    On a initialement utilisé la version Master de la librairie micro-ecc mais on est passé à la version Static par soucis
    de simplicité et de legereté. On modifie également les fichiers source de cette librarie pour effacer les fonctions
    qui nous sont d'aucune utilité pour alléger au maximum le projet.

6.  Pour d'autres détails sur le fonctionnement du code nous vous invitons à consulter le fichier source 'main.c'