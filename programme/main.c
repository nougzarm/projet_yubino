#include <avr/io.h>         // Pour manipuler les registres du microcontrôleur
#include <util/delay.h>     // Pour les fonctions de délai
#include <avr/eeprom.h>     // Pour la gestion de la mémoire eeprom
#include "uECC.h"           // Pour la librairie micro-ecc
#include <stdlib.h>         // Pour rand()
//  Macro et librairie pour le  calcul de UBRR (calcul via la formule crée des problème d'arrondis)
#define BAUD 115200
#include <util/setbaud.h>


/*  SOMMAIRE : 
    1. Macros
    2. Configuration UART (TP4)
    3. Configurations au démarrage
    4. Gestion du bouton et fonction de confirmation
    5. Gestion de la mémoire EEPROM
    6. Fonction (pseudo-)aléatoire pour génération de clé et signature
    7. Fonctions de gestion des resquetes reçues (MakeCredential, ...)
    8. Fonction main
    9. Tests
 */

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                         1. MACROS ET PROTOTYPE(S)                                              |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
int avr_rng(uint8_t *dest, unsigned size);  // Fonction aléatoire pour uECC_make_key() et uECC_sign()

#define LED_PIN PD4     // LED sur la broche 4 (PD4 sur Arduino Uno)
#define BUTTON_PIN PD2  // Bouton poussoir sur la broche 2 (PD2 sur Arduino Uno)

// Tailles des données
#define TAILLE_APP_ID_HASH 20
#define TAILLE_CREDENTIAL_ID 16
#define TAILLE_CLE_PRIVE 21
#define TAILLE_CLE_PUBLIC 40
#define TAILLE_SIGNATURE 40
#define TAILLE_DATA_HASH 20

// Codes requetes
#define COMMAND_LIST_CREDENTIALS 0
#define COMMAND_MAKE_CREDENTIAL 1
#define COMMAND_GET_ASSERTION 2
#define COMMAND_RESET 3

// Codes erreurs
#define STATUS_OK 0
#define STATUS_ERR_COMMAND_UNKNOWN 1
#define STATUS_ERR_CRYPTO_FAILED 2
#define STATUS_ERR_BAD_PARAMETER 3
#define STATUS_ERR_NOT_FOUND 4
#define STATUS_ERR_STORAGE_FULL 5
#define STATUS_ERR_APPROVAL 6

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                       2. CONFIGURATION UART (TP4)                                              |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
void UART__init() {
    // Calcul de UBRR
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
    #if USE_2X
    UCSR0A |= (1 << U2X0); // Double vitesse
    #else
    UCSR0A &= ~(1 << U2X0); // Mode normal
    #endif

    UCSR0B = (1 << RXEN0) | (1 << TXEN0);   // Activation de la réception et de la transmission
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Format de trame : 8 bits de données, 1 bit de stop
}

uint8_t UART__getc() {
    while (!(UCSR0A & (1 << RXC0)));    // Attente jusqu'à réception d'un caractère
    return UDR0;                        // Retourne le caractère reçu
}

void UART__putc(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0))); // Attente jusqu'à ce que le buffer de transmission soit prêt
    UDR0 = data;                      // Envoi du caractère
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                   3. CONFIGURATIONS AU DEMARRAGE                                               |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
//  Diverses configurations au démarrage
void config(){
    //  Initialisation des broches
    DDRD |= (1 << LED_PIN);     // Configurer PD4 comme sortie pour la led
    DDRD &= ~(1 << BUTTON_PIN); // Configurer PD2 comme entrée pour le bouton
    PORTD |= (1 << BUTTON_PIN); // Activer la résistance pull-up interne pour le bouton

    PORTD |= (1 << LED_PIN);    // Allume la LED
    _delay_ms(200);             // Attente
    PORTD &= ~(1 << LED_PIN);   // Éteint la LED
    _delay_ms(500);             // Attente

    //  Configuration USART
    UART__init();   // Initialisation périphérique UART

    PORTD |= (1 << LED_PIN);    // Allume la LED
    _delay_ms(200);             // Attente
    PORTD &= ~(1 << LED_PIN);   // Éteint la LED
    _delay_ms(500);             // Attente

    // Configuration de la fonction aléatoire pour les fonctions de génération de clé et de signature
    uECC_set_rng(avr_rng);  
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                           4. GESTION DU BOUTON ET FONCTION DE CONFIRMATION                                     |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
//  Variables permettant d'évaluer l'état du bouton
volatile uint8_t bouton_etat = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t bouton_compteur = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t bouton_appuie = 0;       // flag indiquant un appui validé
//  Fonction de debounce, permettant de détecter un appuie bouton (Voir partie 5 du TP5)
void debounce() {
    uint8_t current_state = PIND & (1 << BUTTON_PIN);   // Lire l'état actuel du bouton (PD2)

    if (current_state != bouton_etat) {       // Si l'état a changé
        bouton_compteur++;              // Incrémenter le compteur
        if (bouton_compteur >= 4) {     // Si l'état est stable pendant 4 cycles
            bouton_etat = current_state;      // Mettre à jour l'état du bouton
            if (bouton_etat == 0) {     // Si le bouton est stable à l'état bas
                bouton_appuie = 1;      // Signaler un appui validé
            }
            bouton_compteur = 0;    // Réinitialiser le compteur
        }
    } else {
        bouton_compteur = 0;    // Si l'état est constant, réinitialiser
    }
}

//  Fonction de test du bouton (s'arrete pas)
void test_bouton(){
    while(1) {
        debounce();             // Appeler la fonction debounce régulièrement
        if (bouton_appuie) {    // Si un appui validé est détecté
            PORTD ^= (1 << LED_PIN); // Inverser l'état de la LED
            bouton_appuie = 0;  // Réinitialiser le drapeau
        }
        _delay_ms(15);  // 'leger' délai pour limiter la fréquence des vérifications
    }
}

//  Fonction demandant la confirmation de l'utilisateur. (1 = user a confirmé, 0 = user a décliné)
uint8_t demande_confirmation(){
    //  boucle 'for' pour les 10 secondes d'attente (un i correspond à une seconde environ)
    for(int i = 0; i < 10; i++){
        PORTD ^= (1 << LED_PIN);        //  allumer la led pendant 0.5 seconde
        for(int j = 0; j < 33 ; j++){   //  j va jusqu'à 33, car 500/15 = 33
            debounce();
            if(bouton_appuie){
                bouton_appuie = 0;          //  reinitialiser le drapeau
                PORTD ^= (1 << LED_PIN);    //  eteindre la led
                return 1;                   //  confirmation obtenue
            }
            _delay_ms(15);
        }
        PORTD ^= (1 << LED_PIN);     //  eteindre la led pendant 0.5 seconde
        for(int j = 0; j < 33 ; j++){
            debounce();
            if(bouton_appuie){
                bouton_appuie = 0;   //  reinitialiser le drapeau
                return 1;            //  confirmation obtenue
            }
            _delay_ms(15);
        }
    }
    return 0;   //  l'utilisateur n'a pas confirmé
}  

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                       5. GESTION DE LA MEMOIRE EEPROM                                          |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
// Derniere case afin d'indiquer la disponibilité (0 si c'est disponible et 255 si c'est occupé)
#define TAILLE_ENTREE (TAILLE_APP_ID_HASH + TAILLE_CREDENTIAL_ID + TAILLE_CLE_PRIVE + 1)
#define MAX_ENTREES 17  // correspond à 1000/TAILLE_ENTREE = 1000/57

uint8_t EEMEM donnees_eeprom[1000]; // Allocation d'une zone de stockage dans l'eeprom
// Enregistrement du nombre d'entrée dans l'eeprom (pour pas se perdre dans les comptes après un redémarrage)
uint8_t EEMEM compteur_eeprom = 0; 

//  Fonction permettant la sauvegarde d'une entrée dans la mémoire eeprom
uint8_t sauvegarde_entree_eeprom(uint8_t* app_id_hash, uint8_t *credential_id, uint8_t *private_key){
    uint8_t compteur = eeprom_read_byte(&compteur_eeprom);
    if (compteur >= MAX_ENTREES){
        return 0;   // code erreur 'Mémoire pleine'
    }

    // Détection de la position du début de la nouvelle entrée dans 'donnees_eeprom'
    uint16_t case_courante = compteur*TAILLE_ENTREE;

    // Ecriture dans l'eeprom
    eeprom_write_block(app_id_hash, &donnees_eeprom[case_courante], TAILLE_APP_ID_HASH);
    eeprom_write_block(credential_id, &donnees_eeprom[case_courante + TAILLE_APP_ID_HASH], TAILLE_CREDENTIAL_ID);
    eeprom_write_block(private_key, &donnees_eeprom[case_courante + TAILLE_APP_ID_HASH + TAILLE_CREDENTIAL_ID], TAILLE_CLE_PRIVE);

    // Sans oublier de marquer que cette zone est désormais occupée
    eeprom_write_byte(&donnees_eeprom[case_courante + TAILLE_ENTREE - 1], 0xFF);

    // Et de mettre à jour le nombre d'entrées enregistrées dans l'eeprom
    eeprom_write_byte(&compteur_eeprom, compteur + 1);

    return 1; // Enregistrement réussi
}

//  Permet de faire une recherche de clé à partir de l'id app haché (remplit credential_id et private_key)
uint8_t recherche_entree_eeprom(uint8_t* app_id_hash, uint8_t *credential_id, uint8_t *private_key){
    uint8_t compteur = eeprom_read_byte(&compteur_eeprom);
    uint8_t lecture_actuelle;
    uint16_t case_courante;
    uint8_t etat_correspondance;
    for(int i=0; i<compteur; i++){
        PORTD |= (1 << LED_PIN); // Allume la LED
        _delay_ms(200);     // Attente 
        PORTD &= ~(1 << LED_PIN); // Éteint la LED
        _delay_ms(500);     // Attente 

        etat_correspondance = 1;
        case_courante = i*TAILLE_ENTREE;
        for(int j=0; j<TAILLE_APP_ID_HASH; j++){
            lecture_actuelle = eeprom_read_byte(&donnees_eeprom[case_courante + j]);
            if(app_id_hash[j] != lecture_actuelle){
                etat_correspondance = 0;

                PORTD |= (1 << LED_PIN); // Allume la LED
                _delay_ms(200);     // Attente 
                PORTD &= ~(1 << LED_PIN); // Éteint la LED
                _delay_ms(500);     // Attente

                break;
            }
        }
        if(etat_correspondance == 1){
            for(int i=0; i<TAILLE_CREDENTIAL_ID; i++){
                credential_id[i] = eeprom_read_byte(&donnees_eeprom[case_courante + TAILLE_APP_ID_HASH + i]);
            }
            for(int i=0; i<TAILLE_CLE_PRIVE; i++){
                private_key[i] = eeprom_read_byte(&donnees_eeprom[case_courante + TAILLE_APP_ID_HASH + TAILLE_CREDENTIAL_ID + i]);
            }
            return 1;   // Succès de la recherche
        }
    }
        return 0;   // Aucune correspondande
}

//  Suppression de toutes les entrées existantes (pour Reset)
void suppression_entrees_eeprom(){
    uint8_t compteur = eeprom_read_byte(&compteur_eeprom);  // Nombre d'entrées
    for(int i=1; i<=compteur; i++){
        eeprom_write_byte(&donnees_eeprom[i * TAILLE_ENTREE + TAILLE_ENTREE - 1], 0x00);    // Marquage des entrées comme 'vides'
    }
    eeprom_write_byte(&compteur_eeprom, 0x00);  // Sans oublier de mettre le compteur à 0
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                               6. FONCTION (PSEUDO-)ALEATOIRE POUR micro-ecc                                    |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
int avr_rng(uint8_t *dest, unsigned size) {
    for (unsigned i = 0; i < size; i++) {
        dest[i] = rand() % 256; // Générer un octet pseudo-aléatoire
    }
    return 1; // Succès
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                  7. FONCTIONS DE GESTION DES REQUETES RECUES                                   |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
//  MAKE CREDENTIAL -----------------------
void make_credential(){
    uint8_t app_id_hash[TAILLE_APP_ID_HASH];

    // Lecture de : app_id_hash = SHA1(app_id)
    for(int i=0; i<TAILLE_APP_ID_HASH; i++){
        app_id_hash[i] = UART__getc();
    }

    // Mode debug pour savoir quel octet on reçoit en premier.

    /* for(int i=0; i<app_id_hash[0]; i++){
        PORTD ^= (1 << LED_PIN); // Inverser l'état de la LED
        _delay_ms(500);
        PORTD ^= (1 << LED_PIN); // Inverser l'état de la LED
        _delay_ms(200);
    }
    _delay_ms(2000); */

    uint8_t confirmation = demande_confirmation();  // Demande de confirmation à user

    // Si il a confirmé ---> On fait le nécessaire
    if(confirmation == 1){
        uint8_t credential_id[TAILLE_CREDENTIAL_ID];
        uint8_t private_key[TAILLE_CLE_PRIVE];
        uint8_t public_key[TAILLE_CLE_PUBLIC];

        //  (Tentative de) création d'une paire de clés : (clé privée, clé publique)
        if (!uECC_make_key(public_key, private_key)){
            UART__putc(STATUS_ERR_CRYPTO_FAILED);   // Échec de génération de la paire de clés (message erreur)
            return; // Sortie
        }

        /*  La paire de clé a été générée, générons à présent le credential_id
            On a choisi la méthode simple de troncature du app_id_hash
            En étant conscient que ça peut crée des collisions avec d'autres app_id_hash...  */
        for(int i=0; i<TAILLE_CREDENTIAL_ID; i++){
            credential_id[i] = app_id_hash[i];
        }

        //  (Tentative de) Sauvegarde de [app_id_hash, credential, private_key] dans (la suite de) la mémoire EEPROM
        uint8_t statut_sauvegarde = sauvegarde_entree_eeprom(app_id_hash, credential_id, private_key);

        //  Cas de la mémoire pleine ---> code erreur STATUS_ERR_STORAGE_FULL
        if(statut_sauvegarde == 0){
            UART__putc(STATUS_ERR_STORAGE_FULL);
        }

        //  Cas où tout s'est bien passé -> Envoie du MakeCredentialResponse : [STATUS_OK, credential_id, public_key]
        else{
            UART__putc(STATUS_OK);
            for(int i=0; i<TAILLE_CREDENTIAL_ID; i++){
                UART__putc(credential_id[i]);
            }
            for(int i=0; i<TAILLE_CLE_PUBLIC; i++){
                UART__putc(public_key[i]);
            }
        }
    }
    else{
        UART__putc(STATUS_ERR_APPROVAL);   //  Pas de confirmation du user (message erreur)
    }
}

//  LIST CREDENTIALS -----------------------
void list_credentials(){
    uint8_t compteur = eeprom_read_byte(&compteur_eeprom);  // Nombre de données existantes dans la mémoire

    // Listes temporaires
    uint8_t octet_lu;
    uint16_t position_entree_actuelle;

    // Renvoie du message ListCredentialsResponse : [STATUS_OK, count, credential_id, app_id_hash, ... ]
    UART__putc(STATUS_OK); 
    UART__putc(compteur);   
    for(int i=0; i<compteur; i++){
        position_entree_actuelle = i*TAILLE_ENTREE; // On commence par le dernier bloc

        // credential_id
        for(int j=0; j<TAILLE_CREDENTIAL_ID; j++){
            octet_lu = eeprom_read_byte(&donnees_eeprom[position_entree_actuelle + j]);
            UART__putc(octet_lu); 
        }

        // app_id_hash = SHA1(app_id)
        for(int j=0; j<TAILLE_APP_ID_HASH; j++){
            octet_lu = eeprom_read_byte(&donnees_eeprom[position_entree_actuelle + TAILLE_CREDENTIAL_ID + j]);
            UART__putc(octet_lu); 
        }
    }
}

//  GET ASSERTION --------------------------
void get_assertion(){
    uint8_t app_id_hash[TAILLE_APP_ID_HASH];
    // Lecture de SHA1(app_id)
    for(int i=0; i<TAILLE_APP_ID_HASH; i++){
        app_id_hash[i] = UART__getc(); // Lecture du i-ème caractère reçu
    }

    uint8_t clientDataHash[TAILLE_DATA_HASH];
    // Lecture de clientDataHash
    for(int i=0; i<TAILLE_DATA_HASH; i++){
        clientDataHash[i] = UART__getc(); // Lecture du i-ème caractère reçu
    }

    uint8_t confirmation = demande_confirmation();  // Demande de confirmation à user

    if(confirmation == 1){
        uint8_t credential_id[TAILLE_CREDENTIAL_ID];
        uint8_t private_key[TAILLE_CLE_PRIVE];

        //  Recherche d'une entrée correspondant à app_id_hash = SHA1(app_id) 
        uint8_t resultat_recherche = recherche_entree_eeprom(app_id_hash, credential_id, private_key);

        if(resultat_recherche == 1){
            uint8_t signature[TAILLE_SIGNATURE];

            // Cas où on n'arrive pas à signer le message
            if (!uECC_sign(private_key, clientDataHash, signature)){
                UART__putc(STATUS_ERR_CRYPTO_FAILED);   //  Impossible de signer (message erreur)
                return; // Sortie
            }

            // Cas où c'est réussi ---> Envoie de GetAssertionResponse : [STATUS_OK, Credential_id, signature]
            UART__putc(STATUS_OK); 
            for(int i=0; i<TAILLE_CREDENTIAL_ID; i++){
                UART__putc(credential_id[i]);
            }
            for(int i=0; i<TAILLE_SIGNATURE; i++){
                UART__putc(signature[i]);
            }
        }
        else if(resultat_recherche == 0){
            UART__putc(STATUS_ERR_NOT_FOUND);   //  Pas de correspondande (message erreur)
        }
    }
    else{
        UART__putc(STATUS_ERR_APPROVAL);   //  Pas de confirmation du user (message erreur)
    }
}

//  RESET -----------------------------
void command_reset(){
    uint8_t confirmation = demande_confirmation();  // Demande de confirmation à user

    // Si il a confirmé ---> suppression des données
    if(confirmation == 1){
        suppression_entrees_eeprom();   
        UART__putc(STATUS_OK);  // message ResetResponse : [STATUS_OK]
    }
    else{
        UART__putc(STATUS_ERR_APPROVAL);   // Pas de confirmation du user (message erreur)
    }
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                                8. FONCTION MAIN                                                |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
int main() {
    config();           // Configurations du démarrage
    uint8_t action;     // Permet l'évaluation
    while(1){
        action = UART__getc();  // Lecture de la commande reçue
        if(action == COMMAND_LIST_CREDENTIALS){
            list_credentials();
        }
        else if(action == COMMAND_MAKE_CREDENTIAL){
            make_credential();
        }
        else if(action == COMMAND_GET_ASSERTION){
            get_assertion();
        }
        else if(action == COMMAND_RESET){
            command_reset();
        }
    }
    return 0;
}

/*  |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|
    |                                                     9. TESTS                                                   |   
    |----------------------------------------------------------------------------------------------------------------|
    |----------------------------------------------------------------------------------------------------------------|                                                     
 */
// Test du bouton et led
/* int main() {
    config();
    while (1) {
        debounce();             // Appeler la fonction debounce régulièrement
        if (bouton_appuie) {    // Si un appui validé est détecté
            PORTD ^= (1 << LED_PIN); // Inverser l'état de la LED
            bouton_appuie = 0;   // Réinitialiser le drapeau
        }
        _delay_ms(15);       // Délai pour limiter la fréquence des vérifications
    }
    return 0;
} */