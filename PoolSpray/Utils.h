
#ifndef Utils_h
#define Utils_h

// -------------------------------------------------------------------------------------
// FONCTIONS UTILITAIRES POUR L'AFFICHAGE DE CARACTERES ACCENTUES SUR LA CONSOLE
// -------------------------------------------------------------------------------------

extern "C"
{

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
unsigned char utf8ascii(unsigned char ascii);

// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
void utf8asciiChar(char* s);

char* ftoa(char* a, double f, int precision);

}
#endif

