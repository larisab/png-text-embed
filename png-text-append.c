
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern unsigned long crc_calculate(char*, int);

#define PNG_SIG_SIZE 8
const char* png_sig = "\211\120\116\107\015\012\032\012";

int little_endian = 0;

static void
endian_swap(uint32_t* x)
{
  if (little_endian)
    {
      char* buf = (char*)x;
      char tmp;
      tmp = buf[0]; buf[0]=buf[3]; buf[3] = tmp;
      tmp = buf[1]; buf[1]=buf[2]; buf[2] = tmp;
    }
}

static void
inject_text_chunk(char *key, char *content, FILE *stream)
{
  uint32_t length = strlen(key) + 1 + strlen(content);
  char *buffer = malloc(length + 4 + 1);
  
  sprintf(buffer, "tEXt%s", key);
  sprintf(buffer + 4 + strlen(key) + 1, "%s", content);

  uint32_t length_out = length;
  endian_swap(&length_out);
  fwrite(&length_out, 1, 4, stream);
  fwrite(buffer, 1, length + 4, stream);
  
  /* calculate and write CRC sum */
  uint32_t crc_out = crc_calculate(buffer, length + 4);
  endian_swap(&crc_out);
  fwrite(&crc_out, 1, 4, stream);
}

int
main(int argc, char *argv[])
{
  /* is this a little-endian machine? */
  {
    uint32_t endian_test = 1;
    little_endian = ((*(char*)(&endian_test)) != 0);
  }

  FILE *infile = stdin;
  FILE *outfile = stdout;

  int buf_size = 1024;
  char *buffer = malloc(buf_size);

  if (argc > 1) infile = fopen(argv[1], "r");
  assert(infile != NULL);

  char sig[PNG_SIG_SIZE];
  assert (fread(sig, 1, PNG_SIG_SIZE, infile) == PNG_SIG_SIZE);
  assert (strncmp(png_sig, sig, PNG_SIG_SIZE) == 0);

  fwrite(png_sig, 1, PNG_SIG_SIZE, outfile);

  int did_text_chunk = 0;

  while(1) {
    uint32_t length;
    char name[4];
    uint32_t crc;

    int n_read_length = fread(&length, 1, 4, infile);
    if (n_read_length != 4) break;

    endian_swap(&length);
    fread(name, 1, 4, infile);

    /* make sure buffer is large enough to hold contents */
    while (buf_size < length) {
      buf_size *= 2;
      buffer = realloc(buffer, buf_size);
    }

    fread(buffer, length, 1, infile);
    fread(&crc, 1, 4, infile);
    endian_swap(&crc);

    uint32_t length_out = length;
    endian_swap(&length_out);
    uint32_t crc_out = crc;
    endian_swap(&crc_out);

    /* echo chunks to output */
    fwrite(&length_out, 1, 4, outfile);
    fwrite(name, 1, 4, outfile);
    fwrite(buffer, 1, length, outfile);
    fwrite(&crc_out, 1, 4, outfile);

    if (!did_text_chunk) {
      inject_text_chunk("message","testing text chunk injection",outfile);
      did_text_chunk = 1;
    }

  }

  return(0);
}

