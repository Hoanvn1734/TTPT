#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <list>
#include <iterator>
#include <openssl/md5.h>
#include <math.h>
#include <mpi.h>
#include <time.h>
#define FILE_NAME "data.txt"
#define DATA 40
#define RESULT 60

using namespace std;

char character[26] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l'
, 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

map<char, int> dictionary;

void encodePasswordMD5(const char *string, char *mdString) {
    unsigned char digest[16];
    // char mdString[33];

    // encode password into MD5
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, string, strlen(string));
    MD5_Final(digest, &ctx);

    for (int i = 0; i < 16; i++)
        sprintf(&mdString[i * 2], "%02x", (unsigned int) digest[i]);
}

void genPassword(char tempPassword[], int *isPause, int point, int i) {
    map<char, int>::iterator itr;
    if (i == point) {
        if (tempPassword[i] == 'z') {
            *isPause = 1;
            return;
        } else {
            itr = dictionary.find(tempPassword[i]);
            tempPassword[i] = character[itr->second + 1];
        }
    } else {
        if(tempPassword[i] == 'z') {
            tempPassword[i] = 'a';
            genPassword(tempPassword, isPause, point, i + 1);
        } else {
            itr = dictionary.find(tempPassword[i]);
            tempPassword[i] = character[itr->second + 1];
        }
    }
}


void writePassword(int lengthPassword) {
    char tempPassword[lengthPassword];
    for(int i = 0; i < 26; i++) {
        dictionary.insert(pair<char, int>(character[i], i));
    }

    for(int i = 0; i < lengthPassword; i++) {
        tempPassword[i] = 'a';
    }

    ofstream password_file("password.txt");
    int isPause = 0;

    while(password_file.is_open() && !isPause) {
        string str = "";
        for(int i = 0; i < lengthPassword; i++) {
            str += tempPassword[i];
        }
        password_file << str;
        password_file <<"\n";
        genPassword(tempPassword, &isPause, lengthPassword - 1, 0);
    }

    password_file.close();

    // itr = dictionary.find('d');
    // cout<<itr->second<<"\n";

}


void splitData(list<string> *passwordBound, int nunberProcess, int lengthPassword) {
    int totalPassword = pow(26, lengthPassword);
    int partPassword = totalPassword / nunberProcess;
    string line;
    ifstream readfile("password.txt");
    int count = 0;
    for (int i = 0; i < lengthPassword; i++) {
        line += 'a';
    }
    passwordBound->push_back(line);
    
    for(int i = 0; i < nunberProcess - 1; i++) {
        count = (i + 1) * partPassword;
        while(1) {
            line.clear();
            if(count > 26) {
                line += character[(count % 26)];
                count = count / 26;
            }else {
                line += character[count];
                break;
            }
        }
        passwordBound->push_back(line);
    }

    line.clear();
    for (int i = 0; i< lengthPassword; i++) {
        line += 'z';
    }
    passwordBound->push_back(line);
}

char *findPassword(char passwordBegin[], char passwordEnd[], char *mdString, char *tempPasswordFound, int lengthPassword) {
    int isPause = 0;
    tempPasswordFound = NULL;
    char tempMdString[33];
    while(!isPause) {
        encodePasswordMD5(passwordBegin, tempMdString);

        if(!strcmp(tempMdString, mdString)){
            tempPasswordFound = passwordBegin;
            break;
        }
        
        if(!strcmp(passwordBegin, passwordEnd))
            break;

        genPassword(passwordBegin, &isPause, lengthPassword - 1, 0);
    }
    
    return tempPasswordFound;
}

void stringToChar(list<string>::iterator itr, char arrayText[]) {
    string text = *itr;
    for(int i = 0; i < text.length(); i++) {
        arrayText[i] = text[i];
    }
}

void rank0(char *mdString, int lengthPassword) {
    clock_t t1,t2;
    t1 = clock();

    int nunberProcess;
    MPI_Status status;
    MPI_Request request;
    MPI_Comm_size(MPI_COMM_WORLD, &nunberProcess);
    list<string> passwordBound;
    list<string>::iterator itr;
    splitData(&passwordBound, nunberProcess-1, lengthPassword);
    itr = passwordBound.begin();

    char arrayText[lengthPassword];

    for(int i = 1; i < nunberProcess; i++) {
        stringToChar(itr, arrayText);
        MPI_Send(arrayText, lengthPassword, MPI_CHAR, i, DATA, MPI_COMM_WORLD);
        itr++;
        stringToChar(itr, arrayText);
        MPI_Send(arrayText, lengthPassword, MPI_CHAR, i, DATA, MPI_COMM_WORLD);
    }
    
    char *tempPasswordFound;
    string tempPassword;

    tempPasswordFound = NULL;
    char passwordProcess[lengthPassword];

    for (int i = 1; i < nunberProcess; i++) {
        MPI_Recv(passwordProcess, lengthPassword, MPI_CHAR, i, RESULT, MPI_COMM_WORLD, &status);
        if(strcmp(passwordProcess, "No")) {
            cout<<"password found :";
            for (int i = 0; i < lengthPassword; i++) {
                cout<<passwordProcess[i];
            }
            cout<<endl;
            tempPasswordFound = NULL;
        }
    }

    t2 = clock();
    float diff ((float) t2 - (float) t1);
    float seconds = diff / CLOCKS_PER_SEC;
    cout << "Time required = " << seconds << " seconds\n";
}

void ranki(char *mdString, int lengthPassword) {
    char buffBegin[lengthPassword];
    char buffEnd[lengthPassword];
    char *tempPasswordFound;
    MPI_Status status;
    MPI_Request request;

    MPI_Recv(buffBegin, lengthPassword, MPI_CHAR, 0, DATA, MPI_COMM_WORLD, &status);
    MPI_Recv(buffEnd, lengthPassword, MPI_CHAR, 0, DATA, MPI_COMM_WORLD, &status);

    char passwordProcess[lengthPassword];
    char passwordNoFound[] = "No";
    tempPasswordFound = findPassword(buffBegin, buffEnd, mdString, tempPasswordFound, lengthPassword);

    if(tempPasswordFound != NULL) {
        for (int i = 0; i < lengthPassword; i++) {
            passwordProcess[i] = tempPasswordFound[i];
        }
        MPI_Send(passwordProcess, lengthPassword, MPI_CHAR, 0, RESULT, MPI_COMM_WORLD);
    } else {
        MPI_Send(passwordNoFound, lengthPassword, MPI_CHAR, 0, RESULT, MPI_COMM_WORLD);
    }
}



int main(int argc, char **argv) {

    int rank;
    MPI_Init(&argc, &argv);
    
    for(int i = 0; i< 26; i++) {
        dictionary.insert(pair<char, int>(character[i], i));
    }

    char mdString[33];
    int lengthPassword;
    if(argc != 3) return -1;
    sscanf(argv[1], "%s", mdString);
    sscanf(argv[2], "%d", &lengthPassword);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0)
        rank0(mdString, lengthPassword);
    else
        ranki(mdString, lengthPassword);
    
    MPI_Finalize();

    return 0;
}