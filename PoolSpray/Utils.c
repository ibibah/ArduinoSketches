
#include <stdlib.h>
#include <string.h>

// ****** UTF8-Decoder: convert UTF8-string to extended ASCII *******
static unsigned char c1;  // Last character buffer

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
unsigned char utf8ascii(unsigned char ascii)
{
  if ( ascii < 128 )   // Standard ASCII-set 0..0x7F handling
  {
    c1 = 0;
    return ascii;
  }

  // get previous input
  unsigned char last = c1;   // get last char
  c1 = ascii;         // remember actual character

  switch ( last )     // conversion depnding on first UTF8-character
  {
  case 0xC2:
    return  ascii;
    break;
  case 0xC3:
    return  (ascii | 0xC0);
    break;
  case 0x82:
    if ( ascii == 0xAC )
    return 0x80;       // special case Euro-symbol
  }

  // otherwise: return zero, if character has to be ignored
  return 0;
}


// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
void utf8asciiChar(char* s)
{
  int  k = 0;
  char c;
  for ( int i=0; i<strlen(s); i++ )
  {
    c = utf8ascii(s[i]);
    if ( c != 0 )
      s[k++] = c;
  }
  s[k] = 0;
}

char* ftoa(char* a, double f, int precision)
{
  long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};

  char* ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while ( *a != '\0' )
    a++;
 *a++ = '.';
 long desimal = abs((long)((f - heiltal) * p[precision]));
 itoa(desimal, a, 10);
 return ret;
}

