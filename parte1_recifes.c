#include <stdio.h>

#define LARGURA 100
#define SEM_VALOR -1

unsigned int randaux(void)
{
    static long seed = 1;
    return (((seed = seed * 214013L + 2531011L) >> 16) & 0x7fff);
}

int aleatorioEntre(int minimo, int maximo)
{
    unsigned int intervalo;

    intervalo = (unsigned int)(maximo - minimo + 1);
    return (int)(randaux() % intervalo) + minimo;
}

void mostrarMapa(int N, int M, int mapa[LARGURA][LARGURA], int zero)
{
    int i, j;

    printf("\n    ");
    for (j = 0; j < M; j++)
        printf("%3d ", j + 1);

    for (i = 0; i < N; i++) {
        printf("\n%2d: ", i + 1);
        for (j = 0; j < M; j++)
            printf("%3d ", mapa[i][j] - zero);
    }
}

void gerarMapa(int N, int M, int mapa[LARGURA][LARGURA],
               int minimo, int maximo, int limitarValores)
{
    const int deslocamentoLinha[4] = {-1, 1, 0, 0};
    const int deslocamentoColuna[4] = {0, 0, -1, 1};
    int i, j, k, vizinhoLinha, vizinhoColuna;
    int soma, vizinhos, porDefinir, valor;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            mapa[i][j] = SEM_VALOR;
    }

    porDefinir = N * M;
    while (porDefinir > 0) {
        i = aleatorioEntre(0, N - 1);
        j = aleatorioEntre(0, M - 1);

        if (mapa[i][j] == SEM_VALOR) {
            soma = 0;
            vizinhos = 0;

            for (k = 0; k < 4; k++) {
                vizinhoLinha = i + deslocamentoLinha[k];
                vizinhoColuna = j + deslocamentoColuna[k];

                if (vizinhoLinha >= 0 && vizinhoLinha < N &&
                    vizinhoColuna >= 0 && vizinhoColuna < M &&
                    mapa[vizinhoLinha][vizinhoColuna] != SEM_VALOR) {
                    soma += mapa[vizinhoLinha][vizinhoColuna];
                    vizinhos++;
                }
            }

            if (vizinhos == 0)
                valor = aleatorioEntre(minimo, maximo);
            else
                valor = soma / vizinhos + aleatorioEntre(-1, 1);

            if (limitarValores) {
                if (valor < minimo)
                    valor = minimo;
                else if (valor > maximo)
                    valor = maximo;
            }

            mapa[i][j] = valor;

            porDefinir--;
        }
    }
}

void gerarAmfisELixo(int N, int M, int amfis[LARGURA][LARGURA],
                     int lixo[LARGURA][LARGURA])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            amfis[i][j] = aleatorioEntre(0, 9) + aleatorioEntre(0, 9);
            lixo[i][j] = aleatorioEntre(0, 19) + aleatorioEntre(0, 19);
        }
    }
}

void mostrarRecife(int N, int M, int coral[LARGURA][LARGURA],
                   int amfis[LARGURA][LARGURA], int lixo[LARGURA][LARGURA])
{
    printf("Coral:");
    mostrarMapa(N, M, coral, 0);
    printf("\nAmfis:");
    mostrarMapa(N, M, amfis, 0);
    printf("\nLixo:");
    mostrarMapa(N, M, lixo, 0);
    printf("\n");
}

void atualizarCoral(int N, int M, int coral[LARGURA][LARGURA],
                    int lixo[LARGURA][LARGURA])
{
    int i, j, coralAtual, aumento, perda;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            coralAtual = coral[i][j];
            aumento = aleatorioEntre(0, coralAtual) / 2 + 3;
            coralAtual += aumento;

            perda = aleatorioEntre(0, lixo[i][j]) / 2;
            coralAtual -= perda;

            if (coralAtual > 100)
                coralAtual = 100;
            else if (coralAtual < 0)
                coralAtual = 0;

            coral[i][j] = coralAtual;
        }
    }
}

int recifeEsgotado(int N, int M, int coral[LARGURA][LARGURA])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            if (coral[i][j] != 0)
                return 0;
        }
    }

    return 1;
}

void atualizarLixo(int N, int M, int profundidade[LARGURA][LARGURA],
                   int lixo[LARGURA][LARGURA])
{
    const int deslocamentoLinha[4] = {-1, 1, 0, 0};
    const int deslocamentoColuna[4] = {0, 0, -1, 1};
    int lixoNovo[LARGURA][LARGURA];
    int i, j, k, vizinhoLinha, vizinhoColuna;
    int X, movido;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            lixoNovo[i][j] = 0;
    }

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            X = lixo[i][j];

            for (k = 0; k < 4; k++) {
                vizinhoLinha = i + deslocamentoLinha[k];
                vizinhoColuna = j + deslocamentoColuna[k];

                if (X > 0 && vizinhoLinha >= 0 && vizinhoLinha < N &&
                    vizinhoColuna >= 0 && vizinhoColuna < M &&
                    profundidade[vizinhoLinha][vizinhoColuna] > profundidade[i][j]) {
                    movido = aleatorioEntre(1, X) / 4;
                    X -= movido;
                    lixoNovo[vizinhoLinha][vizinhoColuna] += movido;
                }
            }

            lixoNovo[i][j] += X;
        }
    }

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            lixo[i][j] = lixoNovo[i][j];
    }
}

void adicionarLixoDaSuperficie(int N, int M, int lixo[LARGURA][LARGURA])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            lixo[i][j] += aleatorioEntre(0, 1) + aleatorioEntre(0, 1);
    }
}

int ehPrimo(int numero)
{
    int divisor;

    if (numero < 2)
        return 0;

    for (divisor = 2; divisor * divisor <= numero; divisor++) {
        if (numero % divisor == 0)
            return 0;
    }

    return 1;
}

void atualizarAmfis(int N, int M, int profundidade[LARGURA][LARGURA],
                    int coral[LARGURA][LARGURA], int lixo[LARGURA][LARGURA],
                    int amfis[LARGURA][LARGURA])
{
    const int deslocamentoLinha[4] = {-1, 1, 0, 0};
    const int deslocamentoColuna[4] = {0, 0, -1, 1};
    int i, j, k, totalNaCasa, alimentados;
    int X, L, sorteioCoral, sorteioLixo, direcao;
    int vizinhoLinha, vizinhoColuna, aleatorio1, aleatorio2, moveu;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            totalNaCasa = amfis[i][j];
            alimentados = 0;

            for (k = 1; k <= totalNaCasa; k++) {
                if (ehPrimo(k))
                    continue;

                X = coral[i][j];
                L = lixo[i][j];
                sorteioCoral = aleatorioEntre(0, X);
                sorteioLixo = aleatorioEntre(0, L);

                if (sorteioCoral > sorteioLixo) {
                    coral[i][j]--;
                    alimentados++;
                    continue;
                }

                if (aleatorioEntre(0, 1) == 1) {
                    amfis[i][j]--;
                    continue;
                }

                direcao = aleatorioEntre(0, 3);
                vizinhoLinha = i + deslocamentoLinha[direcao];
                vizinhoColuna = j + deslocamentoColuna[direcao];
                moveu = 0;

                if (vizinhoLinha >= 0 && vizinhoLinha < N &&
                    vizinhoColuna >= 0 && vizinhoColuna < M &&
                    coral[vizinhoLinha][vizinhoColuna] > coral[i][j]) {
                    aleatorio1 = aleatorioEntre(0, 10);
                    aleatorio2 = aleatorioEntre(0, 10);

                    if (aleatorio1 + aleatorio2 - 10 >
                        profundidade[vizinhoLinha][vizinhoColuna] - profundidade[i][j]) {
                        amfis[i][j]--;
                        amfis[vizinhoLinha][vizinhoColuna]++;
                        moveu = 1;
                    }
                }

                if (!moveu)
                    amfis[i][j]--;
            }

            if (alimentados > 1)
                amfis[i][j] += aleatorioEntre(0, alimentados - 1) - 1;
        }
    }
}

int main(void)
{
    int S, N, M, dias, parte, dia;
    static int profundidade[LARGURA][LARGURA];
    static int coral[LARGURA][LARGURA];
    static int amfis[LARGURA][LARGURA];
    static int lixo[LARGURA][LARGURA];

    if (scanf("%d %d %d %d %d", &S, &N, &M, &dias, &parte) != 5)
        return 1;

    if (S < 0 || N <= 0 || N > LARGURA ||
        M <= 0 || M > LARGURA || dias < 0)
        return 1;

    while (S-- > 0)
        randaux();

    gerarMapa(N, M, profundidade, 990, 1009, 0);
    if (parte >= 1) {
        gerarMapa(N, M, coral, 0, 100, 1);
        gerarAmfisELixo(N, M, amfis, lixo);
    }

    if (parte == 0) {
        printf("Profundidade:");
        mostrarMapa(N, M, profundidade, 1000);
        printf("\n");
    } else if (parte == 1) {
        mostrarRecife(N, M, coral, amfis, lixo);
    } else if (parte == 2) {
        for (dia = 1; dia <= dias; dia++) {
            atualizarCoral(N, M, coral, lixo);

            if (recifeEsgotado(N, M, coral)) {
                printf("Recife esgotado em %d dias\n", dia);
                break;
            }

            atualizarLixo(N, M, profundidade, lixo);
            adicionarLixoDaSuperficie(N, M, lixo);
            atualizarAmfis(N, M, profundidade, coral, lixo, amfis);
        }

        mostrarRecife(N, M, coral, amfis, lixo);
    }

    return 0;
}
