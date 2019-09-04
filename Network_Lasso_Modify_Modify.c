#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>






#define PI 3.1415926535897932384626
#define NUM_TRAIN 792
#define NUM_TEST 193
#define RANGE 1.0
#define MIN_NUM_NB 5
#define MAX_RUN 1
#define MAX_ITER 1000000
#define TRAIN_RHO 0.06 //step size for solving training problem
#define MU 0.5 //regularization parameter
#define TAULERANCE 0.000001






double deg2rad (double deg){
    return ((deg/180.0)*PI);
}
double rad2deg (double rad){
    return ((rad/PI)*180.0);
}
double geo_distance (double lat1, double long1, double lat2, double long2){
    double theta, dist;
    theta = long1 - long2;
    dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
    dist = acos(dist);
    dist = rad2deg(dist);
    dist = dist*60*1.1515; //unit is stature mile
    //dist = dist*1.609344; //unit is kilometer
    //dist = dist*0.8684; //unit is nautical mile
    return (dist);
}






int main (int argc, char *argv[]){
    //Input the csv file of training data:
    int i;
    int *train_house_id = (int *)malloc(sizeof(int)*NUM_TRAIN);
    double **train_data = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_data[i] = (double *)malloc(sizeof(double)*4);
        train_data[i][3] = 1.0;
    }
    double *train_label = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *train_latitude = (double *)malloc(sizeof(double)*NUM_TRAIN);
    double *train_longitude = (double *)malloc(sizeof(double)*NUM_TRAIN);
    char buf[1024];
    char temp[1024];

    FILE *fp1;
    char  filename1[100] = "housing_price_prediction_train_data.csv";
    if(!(fp1 = fopen(filename1,"r"))){
        printf("Cannot open this file\n");
        exit(0);
    } 
    //skip header
    fgets(buf, 1024, fp1);
//printf("%s\n", buf);
    //read values
    i = 0;
    while (fgets(buf, 1024, fp1) != NULL){
//printf("%s\n", buf);
        if (!(sscanf(buf, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%lf,%lf,%d,%lf,%lf,%lf,%lf", temp, temp, temp, temp, temp, temp, temp, temp, temp, temp, &train_latitude[i], &train_longitude[i], &train_house_id[i], &train_data[i][0], &train_data[i][1], &train_data[i][2], &train_label[i]))){
            printf("Cannot scan this line\n");
            exit(0);
        }
//printf("%d\t%lf\t%lf\t%d\n", i, train_latitude[i], train_longitude[i], train_house_id[i]);
        if (++i >= NUM_TRAIN){
            break;
        }
    }
    fclose(fp1);




    //Construct the graph of training data:
    //calculate distances between each pair of training nodes:
    double **train_distance = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_distance[i] = (double *)malloc(sizeof(double)*NUM_TRAIN);
    }
    int j;
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d\n", i);
        for (j = 0; j < NUM_TRAIN; j++){
            train_distance[i][j] = geo_distance(train_latitude[i], train_longitude[i], train_latitude[j], train_longitude[j]);
        }
    }


    //output the distance matrix:
    //FILE *fp2;
    //char filename2[100] = "train_distance.csv";
    //if(!(fp2 = fopen(filename2,"w"))){
        //printf("Cannot open this file\n");
        //exit(0);
    //}
    //for (i = 0; i < NUM_TRAIN; i++){
        //for (j = 0; j < NUM_TRAIN - 1; j++){
            //fprintf(fp2, "%lf, ", train_distance[i][j]);
        //}
        //fprintf(fp2, "%lf\n", train_distance[i][NUM_TRAIN-1]);
    //}
    //fclose(fp2);

    //count the number of neighbors for each training node
    int *train_neighbor_count = (int *)malloc(sizeof(int)*NUM_TRAIN);
    int train_neighbor_total = 0;
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d: ", i);
        train_neighbor_count[i] = 0;
        for (j = 0; j < NUM_TRAIN; j++){
            if (j != i){
                if (train_distance[i][j] <= RANGE){
                    train_neighbor_count[i]++;
                    train_neighbor_total++;
                }
            }
        }
//printf("%d\n", train_neighbor_count[i]);
    }
//printf("Total number of neighbors is %d.\n", train_neighbor_total);
    

    //Construct edges:
    int train_num_edge;
    train_num_edge = train_neighbor_total/2;
    int **train_edge = (int **)malloc(sizeof(int *)*train_num_edge);
    for (i = 0; i < train_num_edge; i++){
        train_edge[i] = (int *)malloc(sizeof(int)*4);
    }
    double *train_weight = (double *)malloc(sizeof(double)*train_num_edge);
    int k, kk;
    k = 0;
    train_neighbor_total = 0;
    for (i = 0; i < NUM_TRAIN; i++){
//printf("%d: ", i);
        train_neighbor_count[i] = 0;
        for (j = 0; j < NUM_TRAIN; j++){
            if ((j != i) && (train_distance[i][j] <= RANGE)){
                if (i < j){
//if (distance[i][j] < 0.001){
    //printf("%d\t%d\t%lf\n", i, j, train_distance[i][j]);
//}
                    train_edge[k][0] = i; 
                    train_edge[k][1] = j; //edge k connects node i and node j
                    train_edge[k][2] = train_neighbor_count[i]; //node "train_edge[k][1]" is the "train_neighbor_count[i]"_th neighbor of node "train_edge[k][0]"
                    if (train_distance[i][j] < 0.001){
                        train_weight[k] = 10000.0;
                    }
                    else{
                        train_weight[k] = 1.0/train_distance[i][j];
                    }
                    k++;
                    train_neighbor_count[i]++;
                    train_neighbor_total++;
                }
                else{
                    for (kk = 0; kk < k; kk++){
                        if ((train_edge[kk][0] == j) && (train_edge[kk][1] == i)){
                            train_edge[kk][3] = train_neighbor_count[i]; //node "train_edge[k][0]" is the "train_neighbor_count[i]"_th neighbor of node "train_edge[k][1]"
                        }
                    }
                    train_neighbor_count[i]++;
                    train_neighbor_total++;
                }
            }
        }
//printf("%d\n", train_neighbor_count[i]);
    }
//printf("k is %d.\n", k);
//printf("Total number of neighbors is %d.\n", train_neighbor_total);




    //Input the csv file of testing data:
    int *test_house_id = (int *)malloc(sizeof(int)*NUM_TEST);
    double **test_data = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_data[i] = (double *)malloc(sizeof(double)*4);
        test_data[i][3] = 1.0;
    }
    double *test_label = (double *)malloc(sizeof(double)*NUM_TEST);
    double *test_latitude = (double *)malloc(sizeof(double)*NUM_TEST);
    double *test_longitude = (double *)malloc(sizeof(double)*NUM_TEST);

    FILE *fp3;
    char  filename3[100] = "housing_price_prediction_test_data.csv";
    if(!(fp3 = fopen(filename3,"r"))){
        printf("Cannot open this file\n");
        exit(0);
    } 
    //skip header
    fgets(buf, 1024, fp3);
//printf("%s\n", buf);
    //read values
    i = 0;
    while (fgets(buf, 1024, fp3) != NULL){
//printf("%s\n", buf);
        if (!(sscanf(buf, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%lf,%lf,%d,%lf,%lf,%lf,%lf", temp, temp, temp, temp, temp, temp, temp, temp, temp, temp, &test_latitude[i], &test_longitude[i], &test_house_id[i], &test_data[i][0], &test_data[i][1], &test_data[i][2], &test_label[i]))){
            printf("Cannot scan this line\n");
            exit(0);
        }
//printf("%d\t%lf\t%lf\t%d\n", i, test_latitude[i], test_longitude[i], test_house_id[i]);
        if (++i >= NUM_TEST){
            break;
        }
    }
    fclose(fp3);




    //Construct the graph of testing data:
    //calculate distances between each pair of testing node and training node:
    double **test_distance = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_distance[i] = (double *)malloc(sizeof(double)*NUM_TRAIN);
    }
    for (i = 0; i < NUM_TEST; i++){
//printf("%d\n", i);
        for (j = 0; j < NUM_TRAIN; j++){
            test_distance[i][j] = geo_distance(test_latitude[i], test_longitude[i], train_latitude[j], train_longitude[j]);
        }
    }


    //output the distance matrix:
    //FILE *fp4;
    //char  filename4[100] = "test_distance.csv";
    //if(!(fp4 = fopen(filename4,"w"))){
        //printf("Cannot open this file\n");
        //exit(0);
    //}
    //for (i = 0; i < NUM_TEST; i++){
        //for (j = 0; j < NUM_TRAIN - 1; j++){
            //fprintf(fp4, "%lf, ", test_distance[i][j]);
        //}
        //fprintf(fp4, "%lf\n", test_distance[i][NUM_TRAIN-1]);
    //}
    //fclose(fp4);

    //count the number of neighbors for each testing node:
    int *test_neighbor_count = (int *)malloc(sizeof(int)*NUM_TEST);
    int test_neighbor_total = 0;
    for (i = 0; i < NUM_TEST; i++){
//printf("%d: ", i);
        test_neighbor_count[i] = 0;
        for (j = 0; j < NUM_TRAIN; j++){
            if (test_distance[i][j] <= RANGE){
                test_neighbor_count[i]++;
                test_neighbor_total++;
            }
        }
        if (test_neighbor_count[i] < MIN_NUM_NB){
            test_neighbor_total += -test_neighbor_count[i] + MIN_NUM_NB;
            test_neighbor_count[i] = MIN_NUM_NB;
        }
//printf("%d\n", test_neighbor_count[i]);
    }
//printf("Total number of neighbors is %d.\n", test_neighbor_total);

     
     //select neighbors for each testing node:
    double **test_weight = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_weight[i] = (double *)malloc(sizeof(double)*test_neighbor_count[i]);
        for (k = 0; k < test_neighbor_count[i]; k++){
            test_weight[i][k] = 1000000.0;
        }
    }
    int **test_neighbor_index = (int **)malloc(sizeof(int *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_neighbor_index[i] = (int *)malloc(sizeof(int)*test_neighbor_count[i]);
        for (k = 0; k < test_neighbor_count[i]; k++){
            test_neighbor_index[i][k] = 1000000;
        }
    }
    for (i = 0; i < NUM_TEST; i++){
//printf("%d\t%d", i, test_neighbor_count[i]);
        for (j = 0; j < NUM_TRAIN; j++){
            for (k = 0; k < test_neighbor_count[i]; k++){
                if (test_weight[i][k] > test_distance[i][j]){
                    break;
                }
            }
            if (k == test_neighbor_count[i] - 1){
                test_weight[i][k] = test_distance[i][j];
                test_neighbor_index[i][k] = j;
            }
            else{
                for (kk = k; kk < test_neighbor_count[i]-1; kk++){
                    test_weight[i][kk+1] = test_weight[i][kk];
                    test_neighbor_index[i][kk+1] = test_neighbor_index[i][kk];
                }
                test_weight[i][k] = test_distance[i][j];
                test_neighbor_index[i][k] = j;
            }
        }
        for (k = 0; k < test_neighbor_count[i]; k++){
//printf("\t%lf", test_weight[i][k]);
            if (test_weight[i][k] < 0.001){
                test_weight[i][k] = 10000.0;
            }
            else{
                test_weight[i][k] = 1.0/test_weight[i][k];
            }
        }
//printf("\n");
    }
    



    //Open primal variables for training data:
    //decision variables for nodes:
    double **train_x = (double **)malloc(sizeof(double *)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_x[i] = (double *)malloc(sizeof(double)*4);
    }
    //decision variables for edges:
    double ***train_z = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_z[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            train_z[i][j] = (double *)malloc(sizeof(double)*train_neighbor_count[i]);
        }
    }
    //Open dual variables for training data:
    //correctors:
    double ***train_lambda = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_lambda[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            train_lambda[i][j] = (double *)malloc(sizeof(double)*train_neighbor_count[i]);
        }
    }
    //predictors:
    double ***train_p = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        train_p[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            train_p[i][j] = (double *)malloc(sizeof(double)*train_neighbor_count[i]);
        }
    }


    //Open primal variables for testing data:
    //decision variables for nodes:
    double **test_x = (double **)malloc(sizeof(double *)*NUM_TEST);
    for (i = 0; i < NUM_TEST; i++){
        test_x[i] = (double *)malloc(sizeof(double)*4);
    }
    



    //N-block Predictor Corrector Proximal Multiplier Agorithm:
    int iter, run;
    double w; //overall penalty parameter
    double ratio;
    double regression;
    double train_objval;
    double train_residual;
    double ***matrix = (double ***)malloc(sizeof(double **)*NUM_TRAIN);
    for (i = 0; i < NUM_TRAIN; i++){
        matrix[i] = (double **)malloc(sizeof(double *)*4);
        for (j = 0; j < 4; j++){
            matrix[i][j] = (double *)malloc(sizeof(double)*5);
        }
    }
    double temp1, temp2;
    double *mean_square_error = (double *)malloc(sizeof(double)*MAX_RUN);
    

    for (run = 0; run < MAX_RUN; run++){
        w = 5.0;
        //w = pow((double)10, (double)(-2.0 + run*(6.0/(MAX_RUN-1))));

        //Initiate primal variables for training data:
        //decision variables for nodes:
        for (i = 0; i < NUM_TRAIN; i++){
            for (j = 0; j < 4; j++){
                train_x[i][j] = 0.0; //initial value
            }
        }
        //decision variables for edges:
        for (i = 0; i < NUM_TRAIN; i++){
            for (j = 0; j < 4; j++){
                for (k = 0; k < train_neighbor_count[i]; k++){
                    train_z[i][j][k] = 0.0; //initial value
                }
            }
        }
        //Initiate dual variables for training data:
        //correctors:
        for (i = 0; i < NUM_TRAIN; i++){
            for (j = 0; j < 4; j++){
                for (k = 0; k < train_neighbor_count[i]; k++){
                    train_lambda[i][j][k] = 0.0;
                }
            }
        }


        //Training Process:
        for (iter = 0; iter < MAX_ITER; iter++){
printf("%d th iteration:\t", iter);
            train_objval = 0.0;


            //Update dual predictors:
            for (i = 0; i < NUM_TRAIN; i++){
                for (j = 0; j < 4; j++){
                    for (k = 0; k < train_neighbor_count[i]; k++){
                        train_p[i][j][k] = train_lambda[i][j][k] + TRAIN_RHO*(train_x[i][j] - train_z[i][j][k]);
                    }
                }
            }


            //Update primal variables:
            //update decision variables for nodes:
            for (i = 0; i < NUM_TRAIN; i++){
                //Gauss Elimination Method:
                //initiate matrix:
                for (j = 0; j < 4; j++){
                    if (j == 3){
                        for (k = 0; k < 4; k++){
                            if (k == j){
                                matrix[i][j][k] = 2*pow((double)train_data[i][3], (double)2) + 1/TRAIN_RHO;
                            }
                            else{
                                matrix[i][j][k] = 2*train_data[i][j]*train_data[i][k];
                            }
                        }
                    }
                    else{
                        for (k = 0; k < 4; k++){
                            if (k == j){
                                matrix[i][j][k] = 2*pow((double)train_data[i][j], (double)2) + 2*MU + 1/TRAIN_RHO;
                            }
                            else{
                                matrix[i][j][k] = 2*train_data[i][j]*train_data[i][k];
                            }
                        }
                    }
                    matrix[i][j][4] = 2*train_data[i][j]*train_label[i] + (1/TRAIN_RHO)*train_x[i][j];
                    for (kk = 0; kk < train_neighbor_count[i]; kk++){
                        matrix[i][j][4] += -train_p[i][j][kk];
                    }
                }
                //calculate upper triangular matrix:
                for (k = 0; k < 4; k++){
                    for (j = 0; j < 4; j++){
                        if (j > k){
                            ratio = matrix[i][j][k]/matrix[i][k][k];
                            for (kk = 0; kk < 5; kk++){
                                matrix[i][j][kk] += -ratio*matrix[i][k][kk];
                            }
                        }
                    }
                }
                train_x[i][3] = matrix[i][3][4]/matrix[i][3][3];
                //backward substitution:
                for (j = 2; j >= 0; j--){
                    for (k = j+1; k < 4; k++){
                        matrix[i][j][4] += -matrix[i][j][k]*train_x[i][k];
                    }
                    train_x[i][j] = matrix[i][j][4]/matrix[i][j][j];
                }
                //calculate objective function value:
                regression = 0.0;
                for (j = 0; j < 4; j++){
                    regression += train_data[i][j]*train_x[i][j];
                    train_objval += MU*pow((double)train_x[i][j], (double)2);
                }
                train_objval += pow((double)(regression - train_label[i]), (double)2);
            }

            //update decision variables for edges:
            for (i = 0; i < train_num_edge; i++){
                //update decision variables for edges:
                for (j = 0; j < 4; j++){
                    train_z[train_edge[i][0]][j][train_edge[i][2]] = (1/(TRAIN_RHO*(4*w*train_weight[i])))*(TRAIN_RHO*(2*w*train_weight[i] + TRAIN_RHO)*train_z[train_edge[i][0]][j][train_edge[i][2]] + 2*w*train_weight[i]*TRAIN_RHO*train_z[train_edge[i][1]][j][train_edge[i][3]] + (2*w*train_weight[i] + TRAIN_RHO)*train_p[train_edge[i][0]][j][train_edge[i][2]] + 2*w*train_weight[i]*train_p[train_edge[i][1]][j][train_edge[i][3]]);
                    train_z[train_edge[i][1]][j][train_edge[i][3]] = (1/(TRAIN_RHO*(4*w*train_weight[i])))*(2*w*train_weight[i]*TRAIN_RHO*train_z[train_edge[i][0]][j][train_edge[i][2]] + TRAIN_RHO*(2*w*train_weight[i] + TRAIN_RHO)*train_z[train_edge[i][1]][j][train_edge[i][3]] + 2*w*train_weight[i]*train_p[train_edge[i][0]][j][train_edge[i][2]] + (2*w*train_weight[i] + TRAIN_RHO)*train_p[train_edge[i][1]][j][train_edge[i][3]]);
                    train_objval += w*train_weight[i]*pow((double)(train_z[train_edge[i][0]][j][train_edge[i][2]] - train_z[train_edge[i][1]][j][train_edge[i][3]]), (double)2);
                }
            }

            
            //Update dual correctors:
            train_residual = 0.0;
            for (i = 0; i < NUM_TRAIN; i++){
                for (j = 0; j < 4; j++){
                    for (k = 0; k < train_neighbor_count[i]; k++){
                        train_lambda[i][j][k] += TRAIN_RHO*(train_x[i][j] - train_z[i][j][k]);
                        train_residual += pow((double)(train_x[i][j] - train_z[i][j][k]), (double)2);
                    }
                }
            }
            train_residual = sqrt(train_residual/train_neighbor_total);
printf("%lf\t%lf\n", train_objval, train_residual);
            if (train_residual < TAULERANCE){
                break;
            }
        }




        //Testing Process:
        for (i = 0; i < NUM_TEST; i++){
            for (j = 0; j < 4; j++){
                temp1 = 0.0;
                temp2 = 0.0;
                for (k = 0; k < test_neighbor_count[i]; k++){
                    temp1 += test_weight[i][k]*train_x[test_neighbor_index[i][k]][j];
                    temp2 += test_weight[i][k];
                }
                test_x[i][j] = temp1/temp2;
            }
        }


        //Calculate MSE for testing data:
        mean_square_error[run] = 0.0;
        for (i = 0; i < NUM_TEST; i++){
            regression = 0.0;
            for (j = 0; j < 4; j++){
                regression += test_data[i][j]*test_x[i][j];
            }
            mean_square_error[run] += pow((double)(regression - test_label[i]), (double)2);
        }
        mean_square_error[run] = mean_square_error[run]/NUM_TEST;
printf("MSE of %d th run is %lf.\n", run, mean_square_error[run]);
    }
}