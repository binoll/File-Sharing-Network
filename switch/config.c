#include "switch.h"

void read_config(const char* filename, struct config* cfg) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Could not open config file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	char line[BUFFER_SIZE];
	while (fgets(line, sizeof(line), file) != NULL) {
		if (sscanf(line, "modify_flag=%d", &cfg->modify_flag) == 1) {
			continue;
		}

		if (sscanf(line, "urg_ptr_value=%hu", &cfg->urg_ptr_value) == 1) {
			continue;
		}
	}

	fclose(file);
}
