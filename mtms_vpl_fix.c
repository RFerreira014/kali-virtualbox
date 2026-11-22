/*
** file: mtms.c  (** UTF-8 **)
**
** UC: 21111 - SO @ UAb
** e-fólio B 2025-26
** mtms - multi task merge sort
**
** Implementação de um pipeline de tarefas POSIX para ordenar uma lista
** de inteiros usando o algoritmo Merge Sort:
**
**   Ficheiro -> T.Leitora -> [2 buffers ord.] -> T.Ordenadoras -> [buffer fusão] -> T.Fusão
**
** A tarefa Leitora divide os dados em subvetores de até 16 elementos e
** preenche um dos 2 buffers de ordenação de cada vez. As tarefas
** Ordenadoras retiram um subvetor de cada vez, ordenam-no e colocam-no
** no buffer de fusão. A tarefa de Fusão vai fundindo os subvetores
** ordenados até obter o vetor completo ordenado.
**
** O acesso aos buffers partilhados é protegido por mutex. Quando uma
** tarefa não consegue aceder ao buffer pretendido, usa sched_yield()
** para libertar o CPU e tenta de novo (busy waiting).
**
** Botão VPL "Run"  <=> Utilização com dados do utilizador em terminal:
**    ./mtms      # lê da entrada padrão stdin o nome do ficheiro a processar
** Botão VPL "Evaluate" <=> Utilização com redireção de ficheiros automática:
**    ./mtms  <input.txt >output.txt
**
** Compilação VPL (auto):
**    gcc -Wall -std=c99 -D_REENTRANT -o mtms mtms.c -lpthread
**
** Aluno: NNNNN - Nome Apelido
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#define MAXTRF     30     // num. máximo de tarefas ordenadoras
#define MAXN    20000     // num. máximo de inteiros a ordenar
#define MAXSUB     16     // cada subvetor tem no máximo 16 elementos
#define PORLIN     12     // inteiros por linha na saída
#define NBORD       2     // número fixo de buffers de ordenação (ver diagrama)

// vetor de entrada e parâmetros lidos do cabeçalho do ficheiro
int v[MAXN], n_total, nt, dim_ord, dim_fus;
int nsubv; // total de subvetores = ceil(n_total / MAXSUB)

/*
** Buffers de ordenação: 2 buffers fixos, cada um com dim_ord slots.
** Cada slot guarda um apontador para uma secção de v[] e o respetivo tamanho.
**
** bord_ready[b] = 0  ->  buffer livre, a leitora pode preencher
** bord_ready[b] = 1  ->  buffer cheio, as ordenadoras podem consumir
*/
int  **bord_ptr[NBORD];      // apontadores para os subvetores
int   *bord_tam[NBORD];      // tamanho de cada subvetor
int    bord_nfilled[NBORD];  // quantos slots foram preenchidos
int    bord_head[NBORD];     // próximo slot a ser retirado pelas ordenadoras
int    bord_ready[NBORD];    // estado do buffer: 0=livre, 1=pronto
pthread_mutex_t mtx_bord[NBORD];

/*
** Buffer de fusão: dim_fus slots independentes.
** As ordenadoras colocam um subvetor ordenado de cada vez;
** a tarefa de fusão retira um de cada vez e funde com o seu vetor interno.
*/
int  **bfus_ptr;
int   *bfus_tam;
int   *bfus_ocp; // 0=livre, 1=ocupado
pthread_mutex_t mtx_bfus = PTHREAD_MUTEX_INITIALIZER;

// vetor interno da tarefa de fusão, construído incrementalmente
int resultado[MAXN], res_n = 0;

// contador partilhado de subvetores já processados pelas ordenadoras
int subv_consumed = 0;
pthread_mutex_t mtx_count = PTHREAD_MUTEX_INITIALIZER;

// contadores para o relatório final
int rel_leit_ints = 0;
int rel_leit_buf[NBORD]; // subvetores colocados em cada buffer de ordenação
int rel_ord_ints[MAXTRF], rel_ord_bufs[MAXTRF];
int rel_fus_ints = 0, rel_fus_nfus = 0;

/*
** Argumento passado a cada tarefa ordenadora.
** Cada tarefa tem a sua própria seed para que as escolhas aleatórias
** sejam independentes entre tarefas (rand_r é thread-safe ao contrário de rand).
*/
typedef struct { int id; unsigned int seed; } ord_arg_t;

void *tarefa_leitora   (void *arg);
void *tarefa_ordenadora(void *arg);
void *tarefa_fusao     (void *arg);
void  insertion_sort   (int *a, int n);
void  merge_dois       (int *base, int nb, int *sub, int ns);


int main(int argc, char *argv[])
{
    (void)argc; (void)argv; // argumentos não utilizados

    char nome[256];
    int  i, r, b;
    pthread_t      tid_leit, tid_fus, tid_ord[MAXTRF];
    pthread_attr_t attr;
    ord_arg_t      args[MAXTRF];

    // ler o nome do ficheiro da entrada padrão
    if( scanf("%255s", nome) != 1 ) {
        fprintf(stderr, "falta o nome do ficheiro\n");
        return 1;
    }

    // abrir e ler o ficheiro de entrada
    FILE *f = fopen(nome, "r");
    if( !f ) { perror("fopen"); return 1; }

    if( fscanf(f, " nt=%d",                  &nt)      != 1 ||
        fscanf(f, " dim_buffer_ordenacao=%d", &dim_ord) != 1 ||
        fscanf(f, " dim_buffer_fusao=%d",     &dim_fus) != 1 ||
        fscanf(f, " n=%d",                    &n_total) != 1 ) {
        fprintf(stderr, "erro no cabeçalho\n");
        fclose(f);
        return 1;
    }

    // saltar a linha "# dados"
    { char tmp[256]; fgets(tmp, sizeof(tmp), f); fgets(tmp, sizeof(tmp), f); }

    // ler todos os inteiros para o vetor v[]
    for( i = 0; i < n_total; i++ ) {
        if( fscanf(f, "%d", &v[i]) != 1 ) {
            fprintf(stderr, "erro a ler dado %d\n", i);
            fclose(f);
            return 1;
        }
    }
    fclose(f);

    // calcular o total de subvetores (o último pode ter menos de MAXSUB elementos)
    nsubv = (n_total + MAXSUB - 1) / MAXSUB;

    // alocar e inicializar os 2 buffers de ordenação
    for( b = 0; b < NBORD; b++ ) {
        bord_ptr[b]     = malloc(dim_ord * sizeof(int *));
        bord_tam[b]     = malloc(dim_ord * sizeof(int));
        bord_nfilled[b] = 0;
        bord_head[b]    = 0;
        bord_ready[b]   = 0; // começa livre para a leitora
        rel_leit_buf[b] = 0;
        pthread_mutex_init(&mtx_bord[b], NULL);
        if( !bord_ptr[b] || !bord_tam[b] ) { perror("malloc bord"); return 1; }
    }

    // alocar o buffer de fusão (calloc inicia os flags de ocupado a zero)
    bfus_ptr = malloc(dim_fus * sizeof(int *));
    bfus_tam = malloc(dim_fus * sizeof(int));
    bfus_ocp = calloc(dim_fus, sizeof(int));
    if( !bfus_ptr || !bfus_tam || !bfus_ocp ) { perror("malloc bfus"); return 1; }

    memset(rel_ord_ints, 0, sizeof(rel_ord_ints));
    memset(rel_ord_bufs, 0, sizeof(rel_ord_bufs));

    // criar as tarefas como joinable para poder esperar pelo seu fim com pthread_join
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    r = pthread_create(&tid_leit, &attr, tarefa_leitora, NULL);
    if( r ) { perror("criar leitora"); return 1; }

    for( i = 0; i < nt; i++ ) {
        args[i].id   = i;
        args[i].seed = (unsigned int)(i + 1) * 1013904223u; // seed diferente por tarefa
        r = pthread_create(&tid_ord[i], &attr, tarefa_ordenadora, &args[i]);
        if( r ) { perror("criar ordenadora"); return 1; }
    }

    r = pthread_create(&tid_fus, &attr, tarefa_fusao, NULL);
    if( r ) { perror("criar fusão"); return 1; }

    // esperar que todas as tarefas do pipeline terminem
    pthread_join(tid_leit, NULL);
    for( i = 0; i < nt; i++ ) pthread_join(tid_ord[i], NULL);
    pthread_join(tid_fus, NULL);
    pthread_attr_destroy(&attr);

    // libertar recursos
    for( b = 0; b < NBORD; b++ ) {
        pthread_mutex_destroy(&mtx_bord[b]);
        free(bord_ptr[b]);
        free(bord_tam[b]);
    }
    free(bfus_ptr); free(bfus_tam); free(bfus_ocp);

    // relatório final
    printf("nt=%d\n", nt);
    printf("n=%d\n",  n_total);
    printf("T. Leitora:\n");
    printf("%d ints em %d+%d buffers\n",
           rel_leit_ints, rel_leit_buf[0], rel_leit_buf[1]);
    printf("T. Ordenadoras(%d):\n", nt);
    for( i = 0; i < nt; i++ )
        printf("T_%d: %d ints em %d buffers\n", i, rel_ord_ints[i], rel_ord_bufs[i]);
    printf("T. Fusão:\n");
    printf("%d ints em %d fusões\n", rel_fus_ints, rel_fus_nfus);

    // dados ordenados: imprimir sempre o vetor completo, 12 por linha
    printf("# dados ordenados\n");
    for( i = 0; i < res_n; i++ ) {
        if( i > 0 && i % PORLIN == 0 ) printf("\n");
        printf(i % PORLIN == 0 ? "%d" : " %d", resultado[i]);
    }
    printf("\n");

    return 0;
}


/*
** Tarefa Leitora
**
** Percorre o vetor v[] e divide-o em subvetores de até MAXSUB elementos.
** Escolhe um dos 2 buffers de ordenação aleatoriamente; se estiver ainda
** a ser consumido pelas ordenadoras, faz busy waiting com sched_yield().
** Quando consegue acesso, preenche o buffer completamente com apontadores
** diretos para as secções de v[] e marca-o como pronto.
*/
void *tarefa_leitora(void *arg)
{
    (void)arg;
    unsigned int seed = 999983u;
    int subv_pos = 0; // índice do próximo subvetor a colocar

    while( subv_pos < nsubv ) {
        int b = (int)(rand_r(&seed) % (unsigned)NBORD);

        pthread_mutex_lock(&mtx_bord[b]);
        if( bord_ready[b] == 0 ) {
            // buffer livre: preencher com até dim_ord apontadores para v[]
            int s, nfill = 0;
            for( s = 0; s < dim_ord && subv_pos + s < nsubv; s++ ) {
                int offset = (subv_pos + s) * MAXSUB;
                int tam    = n_total - offset;
                if( tam > MAXSUB ) tam = MAXSUB;
                bord_ptr[b][s] = &v[offset]; // apontador direto, sem cópia
                bord_tam[b][s] = tam;
                nfill++;
                rel_leit_ints += tam;
            }
            bord_nfilled[b] = nfill;
            bord_head[b]    = 0;
            bord_ready[b]   = 1; // buffer pronto para as ordenadoras
            pthread_mutex_unlock(&mtx_bord[b]);
            rel_leit_buf[b] += nfill; // contar os subvetores colocados neste buffer
            subv_pos += nfill;
        } else {
            // buffer ocupado: ceder o CPU e tentar de novo
            pthread_mutex_unlock(&mtx_bord[b]);
            sched_yield();
        }
    }

    return NULL;
}


/*
** Tarefa Ordenadora
**
** Cada tarefa escolhe um buffer de ordenação aleatoriamente e tenta
** retirar um subvetor. Se o buffer não tiver dados disponíveis, faz
** busy waiting. Após retirar, ordena o subvetor com insertion sort
** (adequado para blocos de até 16 elementos) e coloca-o no buffer
** de fusão. Termina quando todos os subvetores tiverem sido processados.
*/
void *tarefa_ordenadora(void *arg)
{
    int          id   = ((ord_arg_t *)arg)->id;
    unsigned int seed = ((ord_arg_t *)arg)->seed;

    while(1) {
        // verificar se ainda há subvetores por processar
        pthread_mutex_lock(&mtx_count);
        if( subv_consumed >= nsubv ) { pthread_mutex_unlock(&mtx_count); break; }
        pthread_mutex_unlock(&mtx_count);

        // tentar retirar um subvetor de um buffer de ordenação
        int b    = (int)(rand_r(&seed) % (unsigned)NBORD);
        int *sub = NULL;
        int  nsub = 0;

        pthread_mutex_lock(&mtx_bord[b]);
        if( bord_ready[b] == 1 && bord_head[b] < bord_nfilled[b] ) {
            int s = bord_head[b]++;
            sub   = bord_ptr[b][s];
            nsub  = bord_tam[b][s];
            if( bord_head[b] >= bord_nfilled[b] )
                bord_ready[b] = 0; // buffer esgotado, leitora pode repor
        }
        pthread_mutex_unlock(&mtx_bord[b]);

        if( sub == NULL ) {
            // nenhum slot disponível, ceder o CPU e tentar de novo
            sched_yield();
            continue;
        }

        // ordenar o subvetor in-place (é uma secção de v[])
        insertion_sort(sub, nsub);
        rel_ord_ints[id] += nsub;
        rel_ord_bufs[id]++;

        // registar que mais um subvetor foi processado
        pthread_mutex_lock(&mtx_count);
        subv_consumed++;
        pthread_mutex_unlock(&mtx_count);

        // colocar o subvetor ordenado no buffer de fusão
        int fidx = (int)(rand_r(&seed) % (unsigned)dim_fus);
        while(1) {
            pthread_mutex_lock(&mtx_bfus);
            if( bfus_ocp[fidx] == 0 ) {
                bfus_ptr[fidx] = sub;
                bfus_tam[fidx] = nsub;
                bfus_ocp[fidx] = 1;
                pthread_mutex_unlock(&mtx_bfus);
                break;
            }
            pthread_mutex_unlock(&mtx_bfus);
            sched_yield();
            fidx = (int)(rand_r(&seed) % (unsigned)dim_fus);
        }
    }

    return NULL;
}


/*
** Tarefa Fusão
**
** Retira subvetores já ordenados do buffer de fusão e vai fundindo-os
** com o vetor resultado[], que começa vazio e cresce a cada fusão.
** Termina quando resultado[] tiver todos os n_total inteiros.
*/
void *tarefa_fusao(void *arg)
{
    (void)arg;
    unsigned int seed = 777777u;

    while( res_n < n_total ) {
        int idx = (int)(rand_r(&seed) % (unsigned)dim_fus);

        pthread_mutex_lock(&mtx_bfus);
        if( bfus_ocp[idx] == 1 ) {
            int *sub  = bfus_ptr[idx];
            int  nsub = bfus_tam[idx];
            bfus_ocp[idx] = 0; // libertar o slot
            pthread_mutex_unlock(&mtx_bfus);

            // fundir o subvetor com o resultado acumulado
            merge_dois(resultado, res_n, sub, nsub);
            res_n += nsub;
            rel_fus_ints += nsub;
            rel_fus_nfus++;
            // sub aponta para v[], não foi alocado dinamicamente
        } else {
            pthread_mutex_unlock(&mtx_bfus);
            sched_yield();
        }
    }

    return NULL;
}


/*
** Insertion sort para arrays pequenos (até 16 elementos).
** Percorre o array e insere cada elemento na posição correta
** relativamente aos elementos já ordenados à sua esquerda.
** A ordenação é feita diretamente na secção de v[].
*/
void insertion_sort(int *a, int n)
{
    int i, j, tmp;
    for( i = 1; i < n; i++ ) {
        tmp = a[i];
        j   = i - 1;
        while( j >= 0 && a[j] > tmp ) { a[j+1] = a[j]; j--; }
        a[j+1] = tmp;
    }
}


/*
** Fusão de dois vetores ordenados — operação central do merge sort.
** Intercala base[0..nb-1] com sub[0..ns-1] num array temporário
** e copia o resultado de volta para base[].
*/
void merge_dois(int *base, int nb, int *sub, int ns)
{
    int *tmp = malloc((size_t)(nb + ns) * sizeof(int));
    int i = 0, j = 0, k = 0;
    if( !tmp ) { perror("malloc merge"); return; }
    while( i < nb && j < ns )
        tmp[k++] = (base[i] <= sub[j]) ? base[i++] : sub[j++];
    while( i < nb ) tmp[k++] = base[i++];
    while( j < ns  ) tmp[k++] = sub[j++];
    memcpy(base, tmp, (size_t)k * sizeof(int));
    free(tmp);
}

// EOF
