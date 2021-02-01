#pragma once

/* initializes mt[NN] with a seed */
void init_genrand64(unsigned long long seed);
/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
void init_by_array64(unsigned long long init_key[],
    unsigned long long key_length); 
/* generates a random number on [0, 2^64-1]-interval */
unsigned long long genrand64_int64(void);

double Unif(void); /* Unif[0,1] RNG */
double Unif_a_b(double a, double b);
double Exponential(double mean);
double Normal(double mean, double sd);
