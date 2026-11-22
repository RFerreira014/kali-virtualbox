#define _FILE_OFFSET_BITS 64

/*
** file: mph4.c  (** UTF-8 **)
**
** UC: 21111 - SO @ UAb
** e-fólio A 2025-26
** mph4 - multi process hash 4 bytes
**
** O programa lê o nome do ficheiro pela entrada padrão e recebe em argv[1]
** o número de processos trabalhadores.
**
** Como nesta resolução só são usadas as funções básicas de processos dadas
** na UC, o cálculo da assinatura é feito em 4 passagens pelo ficheiro:
** uma passagem para cada byte do hash final. Em cada passagem, cada filho
** calcula o XOR da sua parte e devolve esse byte no exit status.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Num. máximo processos trabalhadores
#define MAXPROC 8

//Tamanho máximo do nome do ficheiro lido de stdin
#define MAXNAME 1024

/*
** Esta função calcula o XOR da parte do ficheiro que pertence a um filho,
** mas apenas para um dos 4 bytes da assinatura.
** Os blocos são considerados de 4 bytes e, se o último ficar incompleto,
** os bytes em falta são tratados como zeros.
*/
static unsigned char xor_byte_range(const char *filename,
                                    off_t file_size,
                                    off_t start_block,
                                    off_t end_block,
                                    int byte_index)
{
    int fd;
    off_t block;
    unsigned char value;
    unsigned char result;
    ssize_t nread;

    result = 0;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    for (block = start_block; block < end_block; block++) {
        off_t pos = (block * 4) + byte_index;

        if (pos >= file_size) {
            continue;
        }

        if (lseek(fd, pos, SEEK_SET) == (off_t)-1) {
            close(fd);
            return 0;
        }

        nread = read(fd, &value, 1);
        if (nread == 1) {
            result ^= value;
        } else if (nread < 0) {
            close(fd);
            return 0;
        }
    }

    close(fd);
    return result;
}

int main(int argc, char *argv[])
{
    char filename[MAXNAME];
    int np;
    int i;
    int pass;
    int status;
    unsigned int hash;
    struct stat st;
    off_t file_size;
    off_t full_blocks;
    off_t remainder;
    off_t extra_block;
    off_t total_blocks;
    off_t blocks_per_worker;

    /* No botão Run do VPL pode não existir argumento; usa-se um valor por defeito. */
    np = 4;
    if (argc >= 2) {
        np = atoi(argv[1]);
    }

    if (np < 1) {
        np = 1;
    }
    if (np > MAXPROC) {
        np = MAXPROC;
    }

    if (scanf("%1023s", filename) != 1) {
        return 1;
    }

    if (stat(filename, &st) != 0) {
        perror("stat");
        return 1;
    }

    file_size = st.st_size;
    full_blocks = file_size / 4;
    remainder = file_size % 4;
    extra_block = (remainder > 0) ? 1 : 0;
    total_blocks = full_blocks + extra_block;

    /*
    ** A divisão pelos processos é feita com base nos blocos completos
    ** indicados no enunciado, ou seja, usando dim/4.
    ** Se o ficheiro tiver resto, o último processo trata também desse
    ** último bloco incompleto, considerando zeros nos bytes em falta.
    */
    blocks_per_worker = full_blocks / np;

    hash = 0;

    /*
    ** São feitas 4 passagens independentes, uma para cada byte do hash.
    ** Como o pai só aproveita um valor pequeno no wait(), faz-se uma
    ** passagem para cada byte. Em cada passagem, cada filho devolve
    ** o byte calculado no código de saída.
    */
    for (pass = 0; pass < 4; pass++) {
        unsigned int final_byte;

        final_byte = 0;

        for (i = 0; i < np; i++) {
            pid_t pid;
            off_t start_block;
            off_t end_block;

            start_block = i * blocks_per_worker;
            if (i == np - 1) {
                end_block = total_blocks;
            } else {
                end_block = start_block + blocks_per_worker;
            }

            pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                unsigned char partial;

                partial = xor_byte_range(filename, file_size, start_block, end_block, pass);
                _exit((int)partial);
            }
        }

        for (i = 0; i < np; i++) {
            pid_t wpid;

            wpid = wait(&status);
            if (wpid < 0) {
                perror("wait");
                return 1;
            }

            if (WIFEXITED(status)) {
                final_byte ^= (unsigned int)(WEXITSTATUS(status) & 0xFF);
            } else {
                fprintf(stderr, "Erro num filho.\n");
                return 1;
            }
        }

        hash = (hash << 8) | final_byte;
    }

    printf("hash4= %04X %04X\n", (hash >> 16) & 0xFFFF, hash & 0xFFFF);

    return 0;
}
