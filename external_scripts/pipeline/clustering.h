void print_Matrix(double M[50][26], double threshold) {
	int i=0, j=0, a=0;
	const char *amino_acids = "ACDEFGHIKLMNPQRSTVWY";

	printf("PSSM");
	
	for (j=0; j<20; j++) {
		printf(" %c    ", amino_acids[j]);
	}
	printf("\n");
	for (i=0; i<50; i++) {
		printf("%-2d ", i);
		for (j=0; j<20; j++) {
			a = amino_acids[j] - 'A';
			if (M[i][a] > threshold) {
				printf("% 1.2f ", M[i][a]);
			} else {
				printf("  .   ");
			}
		}
		printf("\n");
	}
	printf("\n");
}


// calculate average Kullback-Leibler for 50x26 matrix, given the amino acid composition
double calculate_KL_divergence(double composition[26], double matrix[50][26]) {
	int i, j;
	double KL = 0.0;
	
	for (i=0; i<50; i++) {
		for (j=0;j<26;j++) {
			if (composition[j] > 0.0 && matrix[i][j] > 0.0) {
				KL += matrix[i][j] * (log(matrix[i][j]/composition[j])/log(2));
			}
		}
	}
	KL /= 50.0;

	return KL;	
}

// calculate average Shannon information vector for 50x26 matrix of amino acid frequencies, no background distribution knowledge
void calculate_Shannon_information_vector(double composition[26], double matrix[50][26], double information_vector[50]) {
	int i, j;
	double H_pos = 0.0;
	
	for (i=0; i<50; i++) {
		H_pos = 0.0;
		for (j=0;j<26;j++) {
			if (composition[j] > 0.0 && matrix[i][j] > 0.0) {
				H_pos -= matrix[i][j] * (log(matrix[i][j])/log(2));
			}
		}
		information_vector[i] = log(20)/log(2) - H_pos;
	}
}

// calculate average Shannon information for 50x26 matrix of amino acid frequencies, no background distribution knowledge
double calculate_Shannon_information(double composition[26], double matrix[50][26]) {
	int i, j;
	double H_pos = 0.0;
	double Shannon_I = 0.0;
	
	for (i=0; i<50; i++) {
		H_pos = 0.0;
		for (j=0;j<26;j++) {
			if (composition[j] > 0.0 && matrix[i][j] > 0.0) {
				H_pos += matrix[i][j] * (log(matrix[i][j])/log(2));
			}
		}
		Shannon_I += log(20)/log(2) + H_pos; // I=log20-H,  H=-Sum[p*log(p)] =>  I=log20+Sum[p*log(p)]
	}
	Shannon_I /= 50.0;

	return Shannon_I;
}


void write_Information_Distribution(const char *filename, double composition[26], double matrices[][50][26], char *CM, int n) {
	FILE *fd = NULL;
	fd = fopen(filename, "w");
	assert(fd != NULL);
	int m;
	double I, KL;
	
	fprintf(fd, "profile\tShannon_Information\tKL_divergence\n");

	for (m=0; m<n; m++) {
		if (CM != NULL && CM[m] != 'x') {
			continue;
		}
		
		//average Shannon information for m-th matrix
		I = calculate_Shannon_information(composition, matrices[m]);

		//average Kullback–Leibler divergence for m-th matrix
		KL = calculate_KL_divergence(composition, matrices[m]);
				
		fprintf(fd, "%d\t%f\t%f\n", m, I, KL);
	}
	
	fclose(fd);
}


void write_Freq_Matrices(const char *filename, double matrices[][50][26], char *CM, unsigned int counts[], int n, bool logodds) {
	char *amino_acids = "ACDEFGHIKLMNPQRSTVWY";
	FILE *fd = NULL;
	const int proto_format_version = 2;
	int m=0, i=0, j=0;

	if (n <= 0){
		return;
	}
	
	fd = fopen(filename, "w");
	assert(fd != NULL);
	
	for (m=0; m<n; m++) {
		if (CM != NULL && CM[m] != 'x') {
			continue;
		}
		fprintf(fd, "PROTOTYPE %d\n", proto_format_version);
		fprintf(fd, "BEGIN\n");
		fprintf(fd, "MATRIX ID=%d K=%d\n", m, counts[m]);
	
		fprintf(fd, "50 ");
		for (j=0; j<strlen(amino_acids); j++) {
			fprintf(fd, "       %c ", amino_acids[j]);
		}
		fprintf(fd, "\n");
		for (i=0; i<50; i++) {
			fprintf(fd, "%2d ", i);
			for (j=0; j<strlen(amino_acids); j++) {
	
				if (!logodds && matrices[m][i][amino_acids[j] - 'A'] > 1.0) {
					fprintf(stderr, "Matrix %d is defected at position %d aminoacid %d (%c) with frequency = %1.6lf. Correcting to 1.0\n", 
						m, i, j, amino_acids[j], matrices[m][i][amino_acids[j] - 'A']);
					
					// correcting unexpectedly high frequency, resulting probably from unprecise double calculations on flancs.
					// ABC & DEF => (with overlap on B&E, ABC is more central) =>  A (B+E)/2 C => kABC+kDEF introduces an error on A and C 
					matrices[m][i][amino_acids[j] - 'A'] = 1.0;
				}
	    			
	    			fprintf(fd, "%1.6lf ", matrices[m][i][amino_acids[j] - 'A']);
				//assert(matrices[m][i][amino_acids[j] - 'A'] <= 1.0);
			}
			fprintf(fd, "\n");
		}
		fprintf(fd, "END\n");
	}
	fclose(fd);
}


void write_Iteration_Clusters(const char *filename, char **CM, double *DITV, int N, int iteration) {
	FILE *fd = NULL;
	int i=0, j=0;
	
	fd = fopen(filename, "w");
	assert(fd != NULL);
	
	for (i=0; i<iteration + 1; i++) {
		fprintf(fd, "%5d %03.10lf ", i, DITV[i]);
		for (j=0; j < 2*N; j++) {
			fprintf(fd, "%c", CM[i][j]);
		}
		fprintf(fd,"\n");
	}	
	fclose(fd);	
}

// returns number of loaded matrices
int load_Matrices(const char *filename, double (**pmatrices)[50][26], unsigned int **pcounts, int rho_param, bool reshuffle_matrices) {
	int profile_format = 1;
	char segment[52] = "";
	char *buf = calloc(256, sizeof(char));
	int m=0, n=0, j=0;
	int pos=0, c[20];
	double b[20];
	bool discard_matrix = false;
	FILE *fd = NULL;
	Matrix M;

	//Generated by procedure: find_permutations.R find_permutations(50, 3, 10)
	int random_index[11][50] = 
	{{46,20,41,29,21,26,37,13,17,36,47,8,25,43,9,48,39,24,1,5,34,45,32,18,14,40,38,28,7,6,11,27,0,44,33,19,16,35,23,4,42,49,2,15,30,12,3,10,22,31},
	{26,41,10,42,36,32,44,16,21,5,20,34,37,33,35,0,46,45,19,18,39,9,27,1,7,13,8,14,28,15,4,43,48,12,29,25,24,40,31,30,38,47,23,11,3,22,2,49,17,6},
        {17,10,21,31,40,18,44,39,30,29,33,41,38,34,15,7,5,23,36,35,47,42,25,19,48,11,3,37,20,43,9,46,1,26,28,22,12,0,4,24,27,14,32,8,16,6,45,49,2,13},
	{48,1,42,34,15,25,30,35,38,2,46,43,45,47,33,0,20,11,14,7,21,32,49,23,37,5,13,39,18,36,28,26,9,22,31,41,19,10,24,8,6,17,44,40,16,29,4,3,27,12},
	{35,48,29,36,47,45,40,11,33,24,9,8,32,15,43,4,34,5,21,6,14,30,13,28,2,26,39,19,31,46,37,41,1,18,38,49,10,17,44,23,22,3,27,20,25,16,12,42,7,0},
	{37,2,8,0,19,36,39,20,13,47,22,31,7,48,32,26,11,42,17,41,44,10,5,49,45,40,1,30,38,29,24,18,43,6,15,3,14,35,34,46,4,23,33,12,28,21,16,27,25,9},
	{31,37,39,41,44,12,18,26,47,24,36,16,8,15,43,6,30,49,38,35,14,19,22,9,28,34,48,5,45,32,27,23,25,46,21,42,40,11,17,0,20,29,13,10,7,1,2,4,3,33},
	{2,35,10,39,47,28,31,44,43,29,37,6,46,45,30,1,16,32,5,25,24,18,11,34,26,21,38,14,22,7,20,23,17,42,33,48,0,19,41,49,12,9,40,4,13,8,27,3,36,15},
	{28,30,33,14,11,48,26,46,21,1,35,49,15,43,37,42,24,16,10,13,34,25,44,23,41,12,9,7,4,20,8,5,29,45,39,17,0,3,31,27,19,36,6,22,40,47,2,32,18,38},
	{42,33,40,26,0,4,36,22,37,19,8,17,30,46,31,48,44,29,45,28,15,41,13,47,27,16,10,5,24,3,35,38,21,49,34,43,1,25,39,20,12,2,9,7,14,18,6,32,23,11},
        {27,30,16,49,38,33,11,36,0,32,42,7,26,41,40,44,45,4,21,47,35,12,31,13,28,15,2,19,39,22,3,18,5,34,10,48,43,6,29,24,46,14,37,17,1,25,20,23,8,9}};

	if (*pmatrices != NULL) {
		printf("Input error. load_Matrices expects an unallocated pointer to matrices (NULL)\n");
		return 0;
	}

	fd = fopen(filename, "r");
	assert(fd != NULL);

	while (fgets(buf, 256, fd)) {
		if (1 == sscanf(buf, "PROTOTYPE %d", &profile_format)) {
			if (profile_format != 1 && profile_format != 2) {
				printf("Format error. Wrong prototype format version! Expecting version 1.\n");
				return 0;
			}
		}
		if (strstr(buf, "BEGIN")) {
			discard_matrix = true;
		}
		if (strstr(buf, "END")) {
			if (!discard_matrix) {
				m++;
			}
		}
		if (profile_format == 1) {
			if (1 == sscanf(buf, "MATRIX K=%d", &M.K)) {
				if (M.K < rho_param) {
					discard_matrix = true;
				} else {
					discard_matrix = false;
				}
			}
		}
		if (profile_format == 2) {
			if (2 == sscanf(buf, "MATRIX ID=%d K=%d", &M.id, &M.K)) {
				if (M.K < rho_param) {
					discard_matrix = true;
				} else {
					discard_matrix = false;
				}
			}
		}
	}
		
	*pmatrices = (double (*)[50][26])calloc(2*m, sizeof(double)*50*26);
	assert(m == 0 || *pmatrices != NULL);
	*pcounts = (unsigned int *)calloc(2*m, sizeof(unsigned int));
	assert(m == 0 || *pcounts != NULL);

	rewind(fd);
	
	while (fgets(buf, 256, fd)) {
		if (1 == sscanf(buf, "PROTOTYPE %d", &profile_format)) {
			if (profile_format != 1 && profile_format != 2) {
				printf("Format error. Wrong prototype format version! Expecting version 1.\n");
				return 0;
			}
		}
		if (strstr(buf, "BEGIN")) {
			discard_matrix = true;
		}
		if (strstr(buf, "END")) {
			if (!discard_matrix) {
				memcpy((*pmatrices)[n], &M.freq, sizeof(double[50][26]));
				(*pcounts)[n] = M.K;
				n++;
			}
		}
		if (1 == sscanf(buf, "SEGMENT %s", segment)) {
			strncpy(M.initial_segment, segment, 50);
		}

		if (profile_format == 1) {
			if (1 == sscanf(buf, "MATRIX K=%d", &M.K)) {
				if (M.K < rho_param) {
					discard_matrix = true;
				} else {
					discard_matrix = false;
				}
				M.id = n;
			}
			if (21 == sscanf(buf,
			    "%2d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
			     &pos,&c[0],&c[1],&c[2],&c[3],&c[4],&c[5],&c[6],&c[7],&c[8],&c[9],&c[10],&c[11],&c[12],&c[13],&c[14],&c[15],&c[16],&c[17],&c[18],&c[19])) {
		    	
				if (pos<0 || pos>49) {
					printf("Format error: invalid position %d\n", pos);
					continue;
				}
				for (j=20; j--;) {
					if (c[j]<0) {
						printf("Format error: invalid position %d,%d = %d\n", pos, j, c[j]);
					}

					if (!reshuffle_matrices) {
						M.freq[pos][amino_acids[j] - 'A'] = (double)(c[j]) / M.K;
					} else {
						M.freq[random_index[0][pos]][amino_acids[j] - 'A'] = (double)(c[j]) / M.K;
					}
				}
			}
		}
		if (profile_format == 2) {
			if (2 == sscanf(buf, "MATRIX ID=%d K=%d", &M.id, &M.K)) {
				if (M.K < rho_param) {
					discard_matrix = true;
				} else {
					discard_matrix = false;
				}
			}
			if (21 == sscanf(buf,
			    "%2d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", 
			     &pos,&b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7],&b[8],&b[9],&b[10],&b[11],&b[12],&b[13],&b[14],&b[15],&b[16],&b[17],&b[18],&b[19])) {
		    	
				if (pos<0 || pos>49) {
					printf("Format error: invalid position %d\n", pos);
					continue;
				}
				for (j=20; j--;) {
					if (b[j]<0) {
						printf("Format error: invalid position %d,%d = %lf\n", pos, j, b[j]);
					}

					if (!reshuffle_matrices) {
						M.freq[pos][amino_acids[j] - 'A'] = b[j];
					} else {
						M.freq[random_index[0][pos]][amino_acids[j] - 'A'] = b[j];
					}
				}
			}
		}
	}
	
	fclose(fd);
	free(buf);
	return m;
}

void load_Composition(double composition[26]) {
	int i;
	FILE *fd = fopen("composition.csv", "r");
	assert(fd != NULL);
	
	for (i=0; i<26; i++) {
		composition[i] = 0.0;
	}
	
	char *buf = calloc(256, sizeof(char));
	char *amino_acid = calloc(256, sizeof(char));
	double frequency = 0.0;
	rewind(fd);

	printf("Loading composition from composition.csv file.\n");
	
	while (fgets(buf, 256, fd)) {
		if (2 == sscanf(buf, "%1s,%lf\n", amino_acid, &frequency)) {
			if (amino_acid[0] - 'A' < 26) {
				frequency /= 100.0; // convert to fractions from percents
				printf("Composition of %c = %f\n", amino_acid[0], frequency);
				composition[amino_acid[0] - 'A'] = (double)frequency;
			}
		}	
	}
	
	fclose(fd);
	free(amino_acid);
	free(buf);
}

void calc_LogOdds_Matrix(double composition[26], double matrix[50][26], double logodds[50][26]) {
	static char *amino_acids = "ACDEFGHIKLMNPQRSTVWY";
	int i, j, q;
	// for each amino acid
	for (j=20;j--;) {
		q = amino_acids[j] - 'A';
		//0.001 that's a really small pseudocount just to protect from taking log(0)
			//printf("PC[%d][%c] = %f\n", i, q+'A', pc);
		for (i=50;i--;) {	
			logodds[i][j] = log( (matrix[i][q] + 0.001) / (composition[q] + 0.001));
		}
	}
}

double calc_Distance(double matrix_A[50][26], double matrix_B[50][26], double *result_distance, char *result_shift, char *result_window_shift, double composition[26]) {
	const char *amino_acids = "ACDEFGHIKLMNPQRSTVWY";
	double d = 0.0;
	double tmp = 0.0;

	double sum_over_positions = 0.0;
	double sum_over_aa_pairwise = 0.0;
	char shift = 0;
	char window_shift = 0;
	int i = 0;
	int j = 0;
	int m = 0;
	int n = 0;
	int q = 0;
	double information_A[50];
	double information_B[50];

	*result_distance = 10000000.0; //+inf
	*result_shift = 0;
	*result_window_shift = 0;
	
	calculate_Shannon_information_vector(composition, matrix_A, information_A);
	calculate_Shannon_information_vector(composition, matrix_B, information_B);

	// window 20!	
	for (shift=-29; shift<30; shift++) {
		for (window_shift=0; window_shift<30-abs(shift); window_shift++) {
			i = MAX(0, shift) + window_shift;
			j = MAX(0, shift) + window_shift - shift;
			
			//printf("shift=%d, window_shift=%d , i=%d, j=%d\n", shift, window_shift, i, j);
			
			assert(i >= 0 && i+20 < 50);
			assert(j >= 0 && j+20 < 50);
			
			d = 0.0;
			sum_over_positions = 0.0;
			for (m=0; m<20; m++) { //position offset
				// sum of squares of pairwise distances for each aminoacid on current position _m_
				sum_over_aa_pairwise = 0.0;
				// amino acid
				for (n = 0; n<20; n++) {
					q = amino_acids[n]-'A';
				    	tmp = matrix_A[i+m][q] - matrix_B[j+m][q];
					sum_over_aa_pairwise += tmp*tmp;
				}
				sum_over_positions += (information_A[i+m] * information_B[j+m]) * sqrt(sum_over_aa_pairwise);
			}
			d = sum_over_positions;
			if (d < *result_distance) {
				*result_distance = d;
				*result_shift = shift;
				*result_window_shift = window_shift;
			}
		}
	}

	return *result_distance;
}

void join_Matrices(double A[50][26], int cA, double B[50][26], int cB, double C[50][26], char shift, char window_shift) {
	int i = 0;
	int j = 0;
	int A_shift, B_shift;
	
	assert(cA > 0);
	assert(cB > 0);

	A_shift = window_shift;
	A_shift += (shift > 0)?shift:0;

	B_shift = window_shift;
	B_shift += (shift >= 0)?0:0-shift;
	
	if (A_shift <= B_shift) {
		memcpy(C, A, sizeof(double)*50*26);
		for (i=0; i<30; i++) {
			for(j=26;j--;) {
				C[A_shift+i][j] += B[B_shift+i][j];
				C[A_shift+i][j] /= 2.0;
			}
		}
	} else {
		memcpy(C, B, sizeof(double)*50*26);
		for (i=0; i<30; i++) {
			for(j=26;j--;) {
				C[B_shift+i][j] += A[A_shift+i][j];
				C[B_shift+i][j] /= 2.0;
			}
		}
	}
}

int compare_doubles(const void *p1, const void *p2) {
	if (*(double *)p1 > *(double *)p2) {
		return 1;
	}
	if (*(double *)p1 < *(double *)p2) {
		return -1;
	}
	return 0;
}

void *emalloc(size_t size, const char* description) {
	void * ptr = NULL;

	ptr = malloc(size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(size)/(1024.0*1024.0), description);
		MPI_Abort(MPI_COMM_WORLD, 2);
		exit(2);
	}
	return ptr;
}

void *ecalloc(size_t nmemb, size_t size, const char* description) {
	void *ptr = NULL;

	ptr = calloc(nmemb, size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(nmemb*size)/(1024.0*1024.0), description);
		MPI_Abort(MPI_COMM_WORLD, 2);
		exit(2);
	}
	return ptr;
}

char **eppcalloc(size_t N, size_t M, const char* description) {
	char **ptr = NULL;
	int i;
	
	ptr = calloc(N, sizeof(char *));
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(N*M*sizeof(char))/(1024.0*1024.0), description);
		MPI_Abort(MPI_COMM_WORLD, 2);
		exit(2);
	}

	for (i=0; i<N; i++) {
		ptr[i] = calloc(M, sizeof(char));
		if (ptr[i] == NULL) {
			fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(N*M*sizeof(char))/(1024.0*1024.0), description);
			MPI_Abort(MPI_COMM_WORLD, 2);
			exit(2);
		}
	}
	return ptr;
}

double **eppcallocf(size_t N, size_t M, const char* description) {
	double **ptr = NULL;
	int i;
	
	ptr = calloc(N, sizeof(double *));
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(N*M*sizeof(double))/(1024.0*1024.0), description);
		MPI_Abort(MPI_COMM_WORLD, 2);
		exit(2);
	}

	for (i=0; i<N; i++) {
		ptr[i] = calloc(M, sizeof(double));
		if (ptr[i] == NULL) {
			fprintf(stderr, "Out of memory. Trying to allocate %f MBytes for %s\n", (double)(N*M*sizeof(double))/(1024.0*1024.0), description);
			MPI_Abort(MPI_COMM_WORLD, 2);
			exit(2);
		}
	}
	return ptr;
}

void eppfree(char **ptr, size_t N) {
	int i;
	for (i=0;i<N;i++) {
		if (ptr[i] != NULL) {
			free(ptr[i]);
		}
	}
	free(ptr);
}

void eppfreef(double **ptr, size_t N) {
	int i;
	for (i=0;i<N;i++) {
		if (ptr[i] != NULL) {
			free(ptr[i]);
		}
	}
	free(ptr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////