#include <avr/io.h>       // Pour manipuler les registres du microcontrôleur
#include <util/delay.h>   // Pour les fonctions de délai

#define LED_PIN PD4 // LED sur la broche 13 (PB5 sur Arduino Uno)
#define BUTTON_PIN PD2 // Bouton poussoir sur la broche 2 (PD2 sur Arduino Uno)

//  Variables permettant d'évaluer l'état du bouton
volatile uint8_t bouton_etat = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t bouton_compteur = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t bouton_appuie = 0;       // flag indiquant un appui validé

//  Diverses configurations au démarrage
void config(){
    // Initialisation des broches
    DDRD |= (1 << LED_PIN);    // Configurer PD4 comme sortie pour la led
    DDRD &= ~(1 << BUTTON_PIN);   // Configurer PD2 comme entrée pour le bouton
    PORTD |= (1 << BUTTON_PIN);   // Activer la résistance pull-up interne pour le bouton
}

// Fonction de debounce (Partie 5 du TP5)
void debounce() {
    uint8_t current_state = PIND & (1 << BUTTON_PIN); // Lire l'état actuel du bouton (PD2)

    if (current_state != bouton_etat) {       // Si l'état a changé
        bouton_compteur++;                             // Incrémenter le compteur
        if (bouton_compteur >= 4) {                    // Si l'état est stable pendant 4 cycles
            bouton_etat = current_state;      // Mettre à jour l'état du bouton
            if (bouton_etat == 0) {           // Si le bouton est stable à l'état bas
                bouton_appuie = 1;             // Signaler un appui validé
            }
            bouton_compteur = 0;                       // Réinitialiser le compteur
        }
    } else {
        bouton_compteur = 0;                           // Si l'état est constant, réinitialiser
    }
}


int main() {
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
}