#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

// Structura trimisa de la parinte la copil pentru definirea intervalului
struct Interval {
    int start;
    int end;
};

// Functie auxiliara pentru verificarea primalitatii
bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

int main() {
    const int NUM_CHILDREN = 10;
    const int STEP = 1000;

    // pipe_parent_to_child: parintele trimite intervalul copilului
    // pipe_child_to_parent: copilul trimite numerele prime inapoi la parinte
    int p2c[NUM_CHILDREN][2];
    int c2p[NUM_CHILDREN][2];
    pid_t pids[NUM_CHILDREN];

    // 1. Creare pipes
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        if (pipe(p2c[i]) == -1 || pipe(c2p[i]) == -1) {
            std::cerr << "Eroare la crearea pipe-urilor pentru copilul " << i << std::endl;
            return 1;
        }
    }

    // 2. Creare procese copil
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        pids[i] = fork();

        if (pids[i] < 0) {
            std::cerr << "Eroare la fork pentru procesul " << i << std::endl;
            return 1;
        }

        if (pids[i] == 0) {
            // === COD EXECUTAT IN COPIL ===

            // Inchidere capete de pipe nefolosite de acest copil
            close(p2c[i][1]); // nu scrie in p2c
            close(c2p[i][0]); // nu citeste din c2p

            // Inchidere descriptorii celorlalte procese
            for (int j = 0; j < NUM_CHILDREN; ++j) {
                if (j != i) {
                    close(p2c[j][0]); close(p2c[j][1]);
                    close(c2p[j][0]); close(c2p[j][1]);
                }
            }

            // Citire interval de la parinte
            Interval interval;
            ssize_t bytesRead = read(p2c[i][0], &interval, sizeof(Interval));
            close(p2c[i][0]); // terminare citire

            if (bytesRead == sizeof(Interval)) {
                // Cautare numere prime si trimitere individuala prin pipe
                for (int num = interval.start; num <= interval.end; ++num) {
                    if (isPrime(num)) {
                        write(c2p[i][1], &num, sizeof(int));
                    }
                }
            }

            // Trimite semnalul de sfarsit (-1) sau pur si simplu inchide pipe-ul
            close(c2p[i][1]);
            _exit(0); // Iesire curata din procesul copil
        }
    }

    // === COD EXECUTAT IN PARINTE ===

    // Inchidere capete neutilizate de parinte
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        close(p2c[i][0]); // parintele doar scrie in p2c
        close(c2p[i][1]); // parintele doar citeste din c2p
    }

    // 3. Trimitere intervale catre procesele copil
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        Interval interval;
        interval.start = i * STEP + 1;
        interval.end = (i + 1) * STEP;

        write(p2c[i][1], &interval, sizeof(Interval));
        close(p2c[i][1]); // Inchidere capat de scriere dupa transmitere
    }

    // 4. Colectare si afisare numere prime
    int primeNumber;
    int totalPrimes = 0;

    std::cout << "--- Numere prime gasite (1 - 10000) ---\n";
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        std::cout << "\n[De la Copilul " << i + 1 << " (Interval "
                  << (i * STEP + 1) << " - " << ((i + 1) * STEP) << ")]:\n";

        // Citeste intregi pana cand capatul de scriere al copilului este inchis (EOF)
        while (read(c2p[i][0], &primeNumber, sizeof(int)) > 0) {
            std::cout << primeNumber << " ";
            totalPrimes++;
        }
        std::cout << "\n";
        close(c2p[i][0]); // Inchidere capat de citire
    }

    // 5. Asteptare terminare procese copil pentru a evita procesele zombie
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        waitpid(pids[i], nullptr, 0);
    }

    std::cout << "\nTotal numere prime gasite: " << totalPrimes << " (asteptat: 1229)\n";
    return 0;
}