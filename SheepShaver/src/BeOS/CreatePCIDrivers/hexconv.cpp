#include <stdio.h>

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s <raw file> <C source>\n", argv[0]);
		return 0;
	}

	FILE *fin = fopen(argv[1], "rb");
	if (fin == NULL) {
		printf("Can't open '%s' for reading\n", argv[1]);
		return 0;
	}

	FILE *fout = fopen(argv[2], "w");
	if (fout == NULL) {
		printf("Can't open '%s' for writing\n", argv[2]);
		return 0;
	}

	unsigned char buf[16];
	while (!feof(fin)) {
		fprintf(fout, "\t");
		int actual = fread(buf, 1, 16, fin);
		for (int i=0; i<actual; i++)
			fprintf(fout, "0x%02x, ", buf[i]);
		fprintf(fout, "\n");
	}

	fclose(fin);
	fclose(fout);
	return 0;
}
