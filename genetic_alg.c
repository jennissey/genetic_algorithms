#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int **create_population(int n);
int **create_population_buf(int n);
void free_population(int **population, int n);
int assign_fitness(int *score_buf, int **population, int n,
                   int solution_number);
void select_pair(int **partner1, int **partner2, int **population, int n,
                 int *scores);
void crossover(int *new_chromosome1, int *new_chromosome2, int *partner1,
               int *partner2, float crossover_rate);
void mutation(int *chromosome, float mutation_rate);
void print_solution(int solution_index, int **population);
void print_operation(int solution_index, int **population);

// arguments: solution number, N, crossover rate, mutation rate
int main(int argc, char const *argv[]) {
  int **population;
  int **new_population;
  int solution_number;
  int n;
  float crossover_rate;
  float mutation_rate;
  int *fitness_scores;
  int solution_index = -1;
  int **tmp;

  srand(time(NULL));

  // get arguments
  if (argc != 5) {
    printf("Error: parameters missing\n");
    printf("Usage: genetic_alg <solution number> <N> <crossover rate> "
           "<mutation rate>\n");
    return 1;
  }

  // get arguments
  solution_number = atoi(argv[1]);
  n = atoi(argv[2]);
  printf("n: %d\n", n);
  crossover_rate = strtof(argv[3], NULL);
  printf("crossover: %f\n", crossover_rate);
  mutation_rate = strtof(argv[4], NULL);
  printf("mutation: %f\n", mutation_rate);

  fitness_scores = malloc(n * sizeof(int));
  // create a population of <N> chromosomes
  population = create_population(n);
  new_population = create_population_buf(n);

  // decode each chromosome and assign a fitness score
  // fitness score: how far away from the solution number
  // abs(number from calculation of chromosome - <solution number>)
  // fitness_scores[i] = population[i]
  solution_index =
      assign_fitness(fitness_scores, population, n, solution_number);

  int g = 0;
  // evolve chromosomes until a solution is found
  while (solution_index == -1) {

    // create <N> new chromosomes
    for (int i = 0; i < n - 1; i = i + 2) {
      // roulette wheel selection of two chromosomes
      int *partner1;
      int *partner2;
      select_pair(&partner1, &partner2, population, n, fitness_scores);
      // BREAKPOINT
      // create new chromosome by mixing the two partners
      crossover(new_population[i], new_population[i + 1], partner1, partner2,
                crossover_rate);

      // do mutation of each bit with the possibility of <mutation rate>
      mutation(new_population[i], mutation_rate);
      mutation(new_population[i + 1], mutation_rate);
    }

    // replace old population with new one
    tmp = population;
    population = new_population;
    new_population = tmp;
    g++;

    solution_index =
        assign_fitness(fitness_scores, population, n, solution_number);
  }

  if (solution_index >= 0) {
    print_solution(solution_index, population);
    printf("%d", g);
  }

  free(fitness_scores);
  free_population(population, n);
  free_population(new_population, n);
  return 0;
}

int **create_population_buf(int n) {
  int **population = malloc(n * sizeof(int *));
  int *population_buf = malloc(n * 13 * sizeof(int));
  for (int i = 0; i < n; i++) {
    population[i] = &population_buf[i * 13];
  }
  return population;
}

// create the chromosomes in a way that it starts with value and
// then operator and value alternate
int **create_population(int n) {
  int **population = create_population_buf(n);
  // create <N> random chromosomes with 3 to 13 genes
  // every chromosome is a string of genes that each consist of 4 bits
  for (int i = 0; i < n; i++) {
    // decide how long the chromosome should be(3-13, only uneven numbers)
    int m = (rand() % 11) + 3;
    if (m % 2 != 1) {
      m++;
    }
    for (int j = 0; j < m; j = j + 2) {
      // put a random value into the chromosome (0-9)
      population[i][j] = rand() % 10;
      // put a random operator into the chromosome (10-13)
      population[i][j + 1] = (rand() % 4) + 10;
    }
    // fill up rest of the chromosome with 0s
    for (int j = m; j < 13; j++) {
      population[i][j] = 0;
    }
  }
  return population;
}

void free_population(int **population, int n) {
  free(population[0]);
  free(population);
}

int update_sum(int sum, int operator, int value) {
  if (operator== 10) {
    sum += value;
  }
  if (operator== 11) {
    sum -= value;
  }
  if (operator== 12) {
    sum *= value;
  }
  if (operator== 13 && value != 0) {
    sum /= value;
  }
  return sum;
}

// parse chromosome in a way that only value operator value counts
// when there is no operator after the last value, stop the sum there
// take the first string of operators and values that makes sense
// at first, there has to be a value, after that: after every value there
// has to be an operator, than a value again
// there has to be at least two values and one operator
int assign_fitness(int *score_buf, int **population, int n,
                   int solution_number) {
  // calculate solution for each chromosome
  for (int i = 0; i < n; i++) {

    int sum = 0;
    int operator= 0;
    int value = 0;
    bool operator_defined = false;
    bool value_defined = false;
    bool calculation_found = false;

    // decode each gene for operators or values and sum them up
    // if two values are not connected with an operator, start new sum
    // with the latest value
    for (int j = 0; j < 13; j++) {
      if (population[i][j] < 10) {
        value = population[i][j];
        if (operator_defined) {
          sum = update_sum(sum, operator, value);
          // every operator can only be used once
          operator_defined = false;
          calculation_found = true;
        } else if (!calculation_found) {
          sum = value;
        }
        value_defined = true;
      } else if (value_defined) {
        operator= population[i][j];
        operator_defined = true;
      }
    }

    // calculate score
    score_buf[i] = abs(solution_number - sum);

    // stop when solution found
    if (score_buf[i] == 0) {
      return i;
    }
  }

  return -1;
}

// never pick one chromosome as both partners
// select a pair of chromosomes via roulette selection
void select_pair(int **partner1, int **partner2, int **population, int n,
                 int *score_buf) {
  // assign each chromosome a number range based on
  // their fitness score
  int *end = malloc(sizeof(int) * n);
  int fitness = 0;
  int sum = 0;

  // assign range for first chromosome
  if (score_buf[0] > 0) {
    fitness = 100 / score_buf[0];
  }

  sum += fitness;
  end[0] = fitness;

  // assign range for rest of the chromosomes
  for (int i = 1; i < n; i++) {
    // score_buf[i] = score(population[i])
    if (score_buf[i] > 0) {
      fitness = 1000 / score_buf[i];
    } else {
      fitness = 0;
    }
    sum += fitness;
    end[i] = end[i - 1] + fitness;
  }

  // generate random number out of the range [0,...,sum]
  if (sum != 0) {
    int r1 = rand() % sum;
    int r2 = rand() % sum;

    // select chromosomes with corresponding range
    bool partner1_found = false;
    bool partner2_found = false;
    for (int i = 0; i < n; i++) {
      if (!partner1_found && r1 < end[i]) {
        *partner1 = population[i];
        partner1_found = true;
      } else if (!partner2_found && r2 < end[i]) {
        *partner2 = population[i];
        partner2_found = true;
      }
      if (partner1_found && partner2_found) {
        break;
      }
    }

    // look for errors
    if (!(partner1_found && partner2_found)) {
      printf("\nERROR: no chromosomes selected for breeding\n\n");
      r1 = rand() % 13;
      r2 = rand() % 13;
      *partner1 = population[r1];
      *partner2 = population[r2];
    }
  } else {
    // if none of the chromosomes are fit enough, choose random partners
    int r1 = rand() % 13;
    int r2 = rand() % 13;
    *partner1 = population[r1];
    *partner2 = population[r2];
  }

  free(end);
}

void crossover(int *new_chromosome1, int *new_chromosome2, int *partner1,
               int *partner2, float crossover_rate) {
  // do crossover with the possibility of <crossover rate>
  int r = rand() % 100;
  int crosspoint = 13;

  // chose a random crosspoint in the chromosomes
  if (r < (int)(crossover_rate * 100)) {
    crosspoint = rand() % 13;
  }

  // swap all genes beyond the crosspoint(every gene is one integer)
  for (int i = 0; i < crosspoint; i++) {
    new_chromosome1[i] = partner1[i];
    new_chromosome2[i] = partner2[i];
  }
  for (int i = crosspoint; i < 13; i++) {
    new_chromosome1[i] = partner2[i];
    new_chromosome2[i] = partner1[i];
  }
}

void mutation(int *chromosome, float mutation_rate) {
  // do mutation on gene i with the possibility of <mutation_rate>
  int gene = 0;
  for (int i = 0; i < 13; i++) {
    int r = rand() % 100;
    if (r < (int)(mutation_rate * 100)) {
      gene = chromosome[i];
      if (i % 2 == 0) {
        chromosome[i] = rand() % 10;
      } else {
        chromosome[i] = (rand() % 4) + 10;
      }
    }
  }
}

char decode_operator(int operator) {
  if (operator== 10) {
    return '+';
  }
  if (operator== 11) {
    return '-';
  }
  if (operator== 12) {
    return '*';
  }
  if (operator== 13) {
    return '/';
  }
  return 'u';
}

void print_operation(int solution_index, int **population) {
  printf("\n");
  for (int i = 0; i < 13; i++) {
    // print value
    int value = population[solution_index][i];
    if (value < 10) {
      printf("%d ", value);
    } else {
      printf("%c ", decode_operator(value));
    }
  }
  printf("\n");
}

void print_solution(int solution_index, int **population) {
  printf("\nSolution found:\n\n");
  for (int i = 0; i < 13; i++) {
    // print value
    int value = population[solution_index][i];
    if (value < 10) {
      printf("%d ", value);
    } else {
      printf("%c ", decode_operator(value));
    }
  }
  printf("\n");
}
