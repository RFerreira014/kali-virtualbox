#include <stdio.h>

// Dimensão máxima dos mapas e marca usada enquanto uma casa está por gerar.
#define LARGURA 100
#define SEM_VALOR -1

// Valores usados para identificar a ação escolhida em cada dia.
#define NADA 0
#define COLHER 1
#define CAPTURAR 2
#define LIMPAR 3

// A ordem é importante: cima, baixo, esquerda e direita.
const int deslocamentoLinha[4] = {-1, 1, 0, 0};
const int deslocamentoColuna[4] = {0, 0, -1, 1};

unsigned int randaux(void)
{
    static long seed = 1;
    return (((seed = seed * 214013L + 2531011L) >> 16) & 0x7fff);
}

int aleatorioEntre(int minimo, int maximo)
{
    unsigned int intervalo;

    // Os dois limites fazem parte do intervalo.
    intervalo = (unsigned int)(maximo - minimo + 1);
    return (int)(randaux() % intervalo) + minimo;
}

// Imprime uma matriz; zero permite deslocar os valores, como na profundidade.
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

void mostrarRecife(int N, int M, int coral[LARGURA][LARGURA],
                   int amfis[LARGURA][LARGURA], int lixo[LARGURA][LARGURA])
{
    // A ordem e o formato seguem os mapas apresentados no enunciado.
    printf("Coral:");
    mostrarMapa(N, M, coral, 0);
    printf("\nAmfis:");
    mostrarMapa(N, M, amfis, 0);
    printf("\nLixo:");
    mostrarMapa(N, M, lixo, 0);
    printf("\n");
}

int casaValida(int N, int M, int linha, int coluna)
{
    return linha >= 0 && linha < N && coluna >= 0 && coluna < M;
}

// Gera um mapa por ordem aleatória. limitarValores aplica os limites ao coral.
void gerarMapa(int N, int M, int mapa[LARGURA][LARGURA],
               int minimo, int maximo, int limitarValores)
{
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

                if (casaValida(N, M, vizinhoLinha, vizinhoColuna) &&
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

    // A ordem linha-coluna e a ordem dos sorteios mantêm a sequência do enunciado.
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            amfis[i][j] = aleatorioEntre(0, 9) + aleatorioEntre(0, 9);
            lixo[i][j] = aleatorioEntre(0, 19) + aleatorioEntre(0, 19);
        }
    }
}

// Atualiza o coral e devolve 1 enquanto existir pelo menos uma casa viva.
int evoluirCoral(int N, int M, int coral[LARGURA][LARGURA],
                 int lixo[LARGURA][LARGURA])
{
    int i, j, saude, existeCoral = 0;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            saude = coral[i][j];
            // Nos exemplos do VPL, o crescimento não inclui o +3 referido no texto.
            saude += aleatorioEntre(0, saude) / 2;
            saude -= aleatorioEntre(0, lixo[i][j]) / 2;
            if (saude > 100)
                saude = 100;
            else if (saude < 0)
                saude = 0;
            coral[i][j] = saude;
            if (saude > 0)
                existeCoral = 1;
        }
    }
    return existeCoral;
}

// Escoa o lixo e acrescenta o lixo vindo da superfície.
void atualizarLixo(int N, int M, int profundidade[LARGURA][LARGURA],
                   int lixo[LARGURA][LARGURA])
{
    static int lixoAnterior[LARGURA][LARGURA];
    int i, j, k, vizinhoLinha, vizinhoColuna, disponivel, movido;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            lixoAnterior[i][j] = lixo[i][j];
    }

    /* O cálculo visível no VPL usa este valor do dia anterior e não inclui
       aqui o acréscimo de 10 indicado no texto da atividade. */
    /* Cada casa só escoa o que tinha no início do dia. O lixo recebido hoje
       fica na casa de destino e não volta a ser movido durante esta ronda. */
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            disponivel = lixoAnterior[i][j];
            for (k = 0; k < 4; k++) {
                vizinhoLinha = i + deslocamentoLinha[k];
                vizinhoColuna = j + deslocamentoColuna[k];
                if (disponivel > 0 &&
                    casaValida(N, M, vizinhoLinha, vizinhoColuna) &&
                    profundidade[vizinhoLinha][vizinhoColuna] > profundidade[i][j]) {
                    // Esta forma segue o cálculo usado no exemplo do VPL.
                    movido = (aleatorioEntre(0, disponivel) + 1) / 4;
                    lixo[vizinhoLinha][vizinhoColuna] += movido;
                    lixo[i][j] -= movido;
                    disponivel -= movido;
                }
            }
        }
    }

    // Os casos do VPL usam um sorteio de 0 ou 1 por casa.
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            lixo[i][j] += aleatorioEntre(0, 1);
    }
}

// Trata alimentação, contaminação, movimento e acasalamento dos Amfis.
void atualizarAmfis(int N, int M, int profundidade[LARGURA][LARGURA],
                    int coral[LARGURA][LARGURA], int amfis[LARGURA][LARGURA],
                    int lixo[LARGURA][LARGURA])
{
    static int amfisInicio[LARGURA][LARGURA];
    int i, j, k, direcao, vizinhoLinha, vizinhoColuna, alimentados;

    /* Guarda a população antes de começar a ronda. Os Amfis que chegam a
       outra casa durante o dia não são processados novamente nessa ronda. */
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            amfisInicio[i][j] = amfis[i][j];
    }

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            alimentados = 0;
            /* Nos casos do VPL, todos os índices são processados por ordem,
               incluindo os que o texto identifica como primos. */
            for (k = 0; k < amfisInicio[i][j]; k++) {
                if (aleatorioEntre(0, coral[i][j]) > aleatorioEntre(0, lixo[i][j])) {
                    alimentados++;
                    coral[i][j]--;
                } else if (aleatorioEntre(0, 1) == 1) {
                    amfis[i][j]--;
                } else {
                    /* Só muda se a casa escolhida tiver mais coral e passar no teste
                       da profundidade; se falhar, permanece na casa atual. */
                    direcao = aleatorioEntre(0, 3);
                    vizinhoLinha = i + deslocamentoLinha[direcao];
                    vizinhoColuna = j + deslocamentoColuna[direcao];
                    if (casaValida(N, M, vizinhoLinha, vizinhoColuna) &&
                        coral[vizinhoLinha][vizinhoColuna] > coral[i][j] &&
                        aleatorioEntre(0, 10) + aleatorioEntre(0, 10) - 10 >
                        profundidade[vizinhoLinha][vizinhoColuna] - profundidade[i][j]) {
                        amfis[i][j]--;
                        amfis[vizinhoLinha][vizinhoColuna]++;
                    }
                }
            }
            // Os casos do VPL não subtraem 1 ao resultado do sorteio.
            if (alimentados > 1)
                amfis[i][j] += aleatorioEntre(0, alimentados - 1);
        }
    }
}

// Executa os passos naturais de um dia; devolve 0 se o coral se esgotar.
int simularDia(int N, int M, int profundidade[LARGURA][LARGURA],
               int coral[LARGURA][LARGURA], int amfis[LARGURA][LARGURA],
               int lixo[LARGURA][LARGURA])
{
    if (!evoluirCoral(N, M, coral, lixo))
        return 0;
    atualizarLixo(N, M, profundidade, lixo);
    atualizarAmfis(N, M, profundidade, coral, amfis, lixo);
    return 1;
}

// Conta os vizinhos mais profundos ou menos profundos do que a casa atual.
int contarVizinhos(int N, int M, int profundidade[LARGURA][LARGURA],
                   int linha, int coluna, int maisProfundos)
{
    int k, vizinhoLinha, vizinhoColuna, total = 0;

    for (k = 0; k < 4; k++) {
        vizinhoLinha = linha + deslocamentoLinha[k];
        vizinhoColuna = coluna + deslocamentoColuna[k];
        if (casaValida(N, M, vizinhoLinha, vizinhoColuna)) {
            if (maisProfundos &&
                profundidade[vizinhoLinha][vizinhoColuna] > profundidade[linha][coluna])
                total++;
            else if (!maisProfundos &&
                     profundidade[vizinhoLinha][vizinhoColuna] < profundidade[linha][coluna])
                total++;
        }
    }
    return total;
}

/* Procura o maior valor entre as casas candidatas. Como percorre linhas e
   colunas por ordem e só troca com um valor maior, conserva o primeiro empate. */
int procurarMaximo(int N, int M, int valores[LARGURA][LARGURA],
                   int candidata[LARGURA][LARGURA], int *linha, int *coluna)
{
    int i, j, maximo = 0;

    *linha = -1;
    *coluna = -1;
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            if (candidata[i][j] && valores[i][j] > maximo) {
                maximo = valores[i][j];
                *linha = i;
                *coluna = j;
            }
        }
    }
    return maximo;
}

int contarAcimaDe(int N, int M, int valores[LARGURA][LARGURA], int limite)
{
    int i, j, total = 0;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            if (valores[i][j] > limite)
                total++;
        }
    }
    return total;
}

// Escolhe a ação segundo o procedimento atual dos Aqualin.
int escolherAcaoAtual(int N, int M, int profundidade[LARGURA][LARGURA],
                      int coral[LARGURA][LARGURA], int amfis[LARGURA][LARGURA],
                      int lixo[LARGURA][LARGURA], int *linha, int *coluna)
{
    static int candidata[LARGURA][LARGURA];
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            candidata[i][j] = 1;
    }
    // Nos testes do VPL, a captura também é escolhida quando há mais de 100 Amfis.
    if (procurarMaximo(N, M, amfis, candidata, linha, coluna) > 50)
        return CAPTURAR;
    if (procurarMaximo(N, M, coral, candidata, linha, coluna) > 50)
        return COLHER;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            candidata[i][j] = contarVizinhos(N, M, profundidade, i, j, 1) > 0;
    }
    if (procurarMaximo(N, M, lixo, candidata, linha, coluna) > 0)
        return LIMPAR;
    return NADA;
}

// Escolhe a ação segundo o procedimento sustentável dos Aqualin.
int escolherAcaoSustentada(int N, int M, int profundidade[LARGURA][LARGURA],
                           int coral[LARGURA][LARGURA], int amfis[LARGURA][LARGURA],
                           int lixo[LARGURA][LARGURA], int *linha, int *coluna)
{
    static int candidata[LARGURA][LARGURA];
    int i, j, maximo;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            candidata[i][j] =
                contarVizinhos(N, M, profundidade, i, j, 1) >
                contarVizinhos(N, M, profundidade, i, j, 0) &&
                (amfis[i][j] > 5 || lixo[i][j] > 5);
    }
    if (procurarMaximo(N, M, coral, candidata, linha, coluna) > 0)
        return lixo[*linha][*coluna] > amfis[*linha][*coluna] ? LIMPAR : CAPTURAR;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++)
            candidata[i][j] = 1;
    }
    maximo = procurarMaximo(N, M, coral, candidata, linha, coluna);
    if (maximo > 90 || contarAcimaDe(N, M, coral, 50) >= 10)
        return COLHER;
    return NADA;
}

// Aplica a ação escolhida e soma o alimento recolhido.
void aplicarAcao(int acao, int linha, int coluna, int coral[LARGURA][LARGURA],
                 int amfis[LARGURA][LARGURA], int lixo[LARGURA][LARGURA],
                 int *alimentoCoral, int *alimentoAmfis)
{
    // A casa fica com metade da quantidade; essa metade é o alimento recolhido.
    if (acao == COLHER) {
        coral[linha][coluna] /= 2;
        *alimentoCoral += coral[linha][coluna];
    } else if (acao == CAPTURAR) {
        amfis[linha][coluna] /= 2;
        *alimentoAmfis += amfis[linha][coluna];
    } else if (acao == LIMPAR) {
        lixo[linha][coluna] /= 2;
    }
}

int main(void)
{
    const char *nomeAcao[4] = {"Nada", "Colher", "Capturar", "Limpar"};
    int S, N, M, dias, parte, dia, acao, linha, coluna;
    int alimentoCoral = 0, alimentoAmfis = 0;
    static int profundidade[LARGURA][LARGURA];
    static int coral[LARGURA][LARGURA];
    static int amfis[LARGURA][LARGURA];
    static int lixo[LARGURA][LARGURA];

    if (scanf("%d %d %d %d %d", &S, &N, &M, &dias, &parte) != 5)
        return 1;

    if (S < 0 || N <= 0 || N > LARGURA ||
        M <= 0 || M > LARGURA || dias < 0)
        return 1;

    // A semente de randaux é fixa; S indica quantos valores iniciais descartar.
    while (S-- > 0)
        randaux();

    gerarMapa(N, M, profundidade, 990, 1009, 0);

    if (parte == 0) {
        printf("Profundidade:");
        mostrarMapa(N, M, profundidade, 1000);
        printf("\n");
        return 0;
    }

    gerarMapa(N, M, coral, 0, 100, 1);
    gerarAmfisELixo(N, M, amfis, lixo);

    // Na parte 2 mostra-se o estado inicial, sem avançar dias.
    if (parte == 1)
        dias = 0;

    for (dia = 1; dia <= dias; dia++) {
        // Nas partes 4 e 5, a ação dos Aqualin acontece antes da evolução natural.
        if (parte >= 3) {
            if (parte == 3)
                acao = escolherAcaoAtual(N, M, profundidade, coral, amfis, lixo,
                                         &linha, &coluna);
            else
                acao = escolherAcaoSustentada(N, M, profundidade, coral, amfis, lixo,
                                              &linha, &coluna);
            aplicarAcao(acao, linha, coluna, coral, amfis, lixo,
                        &alimentoCoral, &alimentoAmfis);
            printf("Dia %d: %s", dia, nomeAcao[acao]);
            if (acao != NADA)
                printf(" %d %d", linha + 1, coluna + 1);
            printf(" (alimento: coral %d; amfis %d)\n", alimentoCoral, alimentoAmfis);
        }
        if (!simularDia(N, M, profundidade, coral, amfis, lixo)) {
            printf("Recife esgotado em %d dias\n", dia);
            break;
        }
    }

    mostrarRecife(N, M, coral, amfis, lixo);
    if (parte >= 3)
        printf("Alimento recolhido: coral %d; amfis %d.\n", alimentoCoral, alimentoAmfis);

    return 0;
}
