#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

bool estePrim(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d) {
        if (n % d == 0) return false;
    }
    return true;
}

void ruleazaCopil(HANDLE hReadPipe, HANDLE hWritePipe) {
    int start = 0, stop = 0;
    DWORD bytesRead;

    if (ReadFile(hReadPipe, &start, sizeof(int), &bytesRead, NULL) &&
        ReadFile(hReadPipe, &stop, sizeof(int), &bytesRead, NULL)) {

        for (int i = start; i <= stop; ++i) {
            if (estePrim(i)) {
                DWORD bytesWritten;
                WriteFile(hWritePipe, &i, sizeof(int), &bytesWritten, NULL);
            }
        }
    }

    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
}

int main(int argc, char* argv[]) {
    // Daca procesul a primit argumente de tip handle, ruleaza ca worker
    if (argc == 3) {
        HANDLE hReadPipe = (HANDLE)(uintptr_t)std::stoull(argv[1]);
        HANDLE hWritePipe = (HANDLE)(uintptr_t)std::stoull(argv[2]);
        ruleazaCopil(hReadPipe, hWritePipe);
        return 0;
    }

    const int NUMAR_PROCESE = 10;
    const int PAS = 1000;

    HANDLE p2c_read[NUMAR_PROCESE], p2c_write[NUMAR_PROCESE];
    HANDLE c2p_read[NUMAR_PROCESE], c2p_write[NUMAR_PROCESE];
    PROCESS_INFORMATION pi[NUMAR_PROCESE];

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    char caleExecutabil[MAX_PATH];
    GetModuleFileNameA(NULL, caleExecutabil, MAX_PATH);

    for (int i = 0; i < NUMAR_PROCESE; ++i) {
        CreatePipe(&p2c_read[i], &p2c_write[i], &sa, 0);
        CreatePipe(&c2p_read[i], &c2p_write[i], &sa, 0);

        // Capetele utilizate de parinte nu trebuie mostenite de copii
        SetHandleInformation(p2c_write[i], HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(c2p_read[i], HANDLE_FLAG_INHERIT, 0);

        std::string cmd = std::string("\"") + caleExecutabil + "\" " +
                          std::to_string((uintptr_t)p2c_read[i]) + " " +
                          std::to_string((uintptr_t)c2p_write[i]);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi[i], sizeof(pi[i]));

        CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi[i]);

        // Inchidere duplicate locale neutilizate de parinte
        CloseHandle(p2c_read[i]);
        CloseHandle(c2p_write[i]);
    }

    // Trimitere intervale
    for (int i = 0; i < NUMAR_PROCESE; ++i) {
        int start = i * PAS + 1;
        int stop = (i + 1) * PAS;
        DWORD bytesWritten;

        WriteFile(p2c_write[i], &start, sizeof(int), &bytesWritten, NULL);
        WriteFile(p2c_write[i], &stop, sizeof(int), &bytesWritten, NULL);
        CloseHandle(p2c_write[i]);
    }

    // Colectare si afisare numere prime
    int total = 0;
    for (int i = 0; i < NUMAR_PROCESE; ++i) {
        int start = i * PAS + 1;
        int stop = (i + 1) * PAS;

        std::cout << "\n--- Numere prime gasite in [" << start << ", " << stop << "] ---\n";

        int prim;
        DWORD bytesRead;
        while (ReadFile(c2p_read[i], &prim, sizeof(int), &bytesRead, NULL) && bytesRead > 0) {
            std::cout << prim << " ";
            total++;
        }
        std::cout << "\n";

        CloseHandle(c2p_read[i]);
        WaitForSingleObject(pi[i].hProcess, INFINITE);
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

    std::cout << "\nTotal numere prime: " << total << std::endl;
    return 0;
}