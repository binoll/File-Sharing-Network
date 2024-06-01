#include "switch.h"

int read_config(const char* filename, struct config* cfg) {
	FILE* file = fopen(filename, "r");
	char line[BUFFER_SIZE];

	if (file == NULL) {
		fprintf(stderr, "Could not open config file %s\n", filename);
		return EXIT_FAILURE;
	}

	while (fgets(line, sizeof(line), file) != NULL) {
		if (sscanf(line, "modify_flag=%d", &cfg->modify_flag) == 1) {
			continue;
		}

		if (sscanf(line, "urg_ptr_value=%hu", &cfg->urg_ptr_value) == 1) {
			continue;
		}
	}

	fclose(file);
	return EXIT_SUCCESS;
}
