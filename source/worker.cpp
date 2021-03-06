#include <iostream>
#include <bits/stdc++.h>
#include <string>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <cstdlib>
#include <fcntl.h>   
#include <unistd.h> 
#include <sys/wait.h>
#include <dirent.h> 
#include <fstream>
#include <signal.h>
#include "../headers/helpworker.h"

using namespace std;

//Global variables which are going to help us to find number of patients who have a specific property.
int still_patients=0;
int suspects=0;
volatile sig_atomic_t sigintorquit=false;
volatile sig_atomic_t sigusr1=false;

int main(int argc,char* argv[])
{
    //We pass to executable of worker.cpp 2 arguments: argv[1] has PC_pipe's name.,argv[2] has buffersize.
    //These are going to help each Worker communicate for first time with Father!
    cout << "Worker " << getpid() << " from " << argv[1] << endl;
    
    //Initialization of crucial variables.
    int bytes=0,msg_bytes=0,PCfd,CPfd,buffersize=atoi(argv[2]),i=0,j=0,k=0,z=0,q=0,num_of_dirs,count=0,records_per_file=0,date_files_per_dir=0,diseases=0,err=0,disease_bucket=0,country_bucket=0,bucket_size=20,last_bytes=0,chunk_times=0;
    char PCfifo[20],CPfifo[20],buff[buffersize],dirs[10],input_dir[20],country[30],path_to_dir[30],path_to_date_file[50];
    char** countries_array,*whole_message,*temp;
    Node* head_of_diseases = NULL;
    RecordNode* head = NULL;
    struct dirent *entry;
    DIR *dir;
    FILE* fptr = NULL;
    string line,word,parameters[8],request,message;
    string** date_files,*latest_dates,**records_array;
    fstream newfile;
    strcpy(country,"");strcpy(path_to_dir,"");strcpy(path_to_date_file,"");strcpy(buff,"");strcpy(input_dir,"");strcpy(dirs,"");strcpy(CPfifo,"");strcpy(PCfifo,argv[1]);
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////Worker reads infos from Father for very first time////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    //Worker opes PC_pipe so as to read for first time some significant values which he'll need later.
    if ((PCfd = open(PCfifo, O_RDONLY ))<0)   perror("Worker cannot open PCfifo!");

    //First read bytes bytes and allocate appropriate memory.
    bytes = read(PCfd,buff,buffersize);
    whole_message = (char*)malloc((bytes+1)*sizeof(char));
    strcpy(whole_message,"");
    strncat(whole_message,buff,bytes);
    msg_bytes=bytes;

    //Worker reads Father's message by buffersize-buffersize bytes, using realloc so as to allocate dynamically memory.
    while((bytes=read(PCfd,buff,buffersize))>0)
    {
        msg_bytes+=bytes;
        whole_message = (char*)realloc(whole_message,(msg_bytes+1)*sizeof(char));
        strncat(whole_message,buff,bytes);
    }

    //Worker closes PC_pipe.
    close(PCfd);
    
    //Store infos to variable...and then Worker is ready to manipulate its dirs.
    for(k=0;k<msg_bytes;k++)    
    {
        //First line of whole message has number of directories of Worker.
        if(j==0)
        {
            if(whole_message[k]=='\n') 
            {
                num_of_dirs = stoi(dirs);
                countries_array = new char*[num_of_dirs];
                j++;
            }
            else    strncat(dirs,&whole_message[k],1);
        }
        //Second line of whole message has names of directories of Worker.
        else if(j==1)
        {
            if(whole_message[k]=='\n') 
            {
                countries_array[i] = new char[strlen(country)+1];
                strcpy(countries_array[i],country);
                strcpy(country,"");
                j++;
            }
            else if(whole_message[k]==' ')    
            {
                countries_array[i] = new char[strlen(country)+1];
                strcpy(countries_array[i],country);
                strcpy(country,"");
                i++;
            }
            else    strncat(country,&whole_message[k],1);   
        }
        //Third line of whole message has name of input_dir.
        else if(j==2)
        {
            if(whole_message[k]=='\n') j++;
            else    strncat(input_dir,&whole_message[k],1);
        }
        //Fourth line of whole message has name of CPfifo.
        else
        {
            if(whole_message[k]=='\n')  j++;
            else    strncat(CPfifo,&whole_message[k],1);
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////Start to read records from date files of each dir////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    //Firstly calculate how many date files has each subdirectory and how many records each date file has only once,also number of diseases.
    for(i=0;i<num_of_dirs;i++)
    {
        strcpy(path_to_dir,input_dir);
        strncat(path_to_dir,"/",1);
        strncat(path_to_dir,countries_array[i],strlen(countries_array[i]));
        date_files_per_dir=0;
        dir = opendir(path_to_dir);
        if(dir==0)  cout << "The parameter input_dir that you gave doesn't exist.Try again!" << endl;
        while ((entry = readdir(dir)) != NULL)  
        {
            if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0)  
            {
                strcpy(path_to_date_file,path_to_dir);
                strncat(path_to_date_file,"/",1);
                strncat(path_to_date_file,entry->d_name,strlen(entry->d_name));
                newfile.open(path_to_date_file,ios::in); 
                if(!newfile.is_open())  cout << "Cannot open date file " << path_to_date_file << endl;

                date_files_per_dir++;
                records_per_file=0;
                while(getline(newfile,line))    
                { 
                    records_per_file++;
                    stringstream iss(line);j=0;
                    while (iss >> parameters[j])    j++;

                    //Case: Record has exactly 6 words so it has the right format.
                    if(j==6)
                    {
                        //Store to a list unique names of diseases of all records of all dirs of this Worker.
                        insert(&head_of_diseases,parameters[4]);
                    }
                }
                newfile.close();
            } 
        }
        closedir(dir);
    }

    //Store number of diseases cause it will help us with creation of diseaseHashTable.
    diseases = length(head_of_diseases);
    //Declaration of Hash Tables (for disease and for country) and initialization with NULL pointers.
    BucketNode* diseaseHashTable[diseases];
    BucketNode* countryHashTable[num_of_dirs];
    for(j=0;j<diseases;j++) diseaseHashTable[j]=NULL;
    for(j=0;j<num_of_dirs;j++) countryHashTable[j]=NULL;
    //Allocate memory for some vital arrays which will help us.
    date_files = new string*[num_of_dirs];
    latest_dates = new string[num_of_dirs];
    records_array = new string*[date_files_per_dir*records_per_file];

    //Loop through directories so as to take records from date files.
    for(i=0;i<num_of_dirs;i++)
    {
        //Prepare path_to_dir so as to open each subdir...
        strcpy(path_to_dir,input_dir);
        strncat(path_to_dir,"/",1);
        strncat(path_to_dir,countries_array[i],strlen(countries_array[i]));

        //Allocate array of appropriate number of strings.
        date_files[i] = new string[date_files_per_dir];
        
        //Firstly store names of date files so as to sort them.
        //Open each subdirectory of Worker.
        dir = opendir(path_to_dir);
        q=0;
        count=0;
        if(dir==0)  cout << "The parameter input_dir that you gave doesn't exist.Try again!" << endl;
        //Take each date file of subdirectory of Worker.
        while ((entry = readdir(dir)) != NULL)  if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0)   date_files[i][count++]=entry->d_name;
        //Close each subdirectory of Worker.
        closedir(dir);
        
        //Sort array of date files in ascending order.
        selectionsort(date_files[i],date_files_per_dir);
        //Store latest date's file of this directory.
        latest_dates[i] = date_files[i][date_files_per_dir-1];
        
        //Now Worker is ready to open date files with ascending order.
        for(k=0;k<date_files_per_dir;k++)
        {
            //Prepare path_to_date to give is as argument to fstream.open().
            strcpy(path_to_date_file,path_to_dir);
            strncat(path_to_date_file,"/",1);
            strncat(path_to_date_file,date_files[i][k].c_str(),strlen(date_files[i][k].c_str()));

            //Open each date file of this subdirectory and take its contents,records.
            newfile.open(path_to_date_file,ios::in); 
            if(!newfile.is_open())  cout << "Cannot open date file " << path_to_date_file << endl;

            //Get line by line each record and store temporarily each contents to array of strings,parameters[7].
            while(getline(newfile, line))
            { 
                stringstream iss(line);j=0;
                while (iss >> parameters[j])    j++;

                //Case: Record has exactly 6 words so it has the right format.
                if(j==6)
                {
                    //Allocate memory for array of 8 strings so as to put inside contents of a full record.
                    records_array[q] = new string[8];
                    
                    //Initialization of contents of record to records_array.
                    records_array[q][0] = parameters[0]; //rID
                    records_array[q][1] = parameters[2]; //first name
                    records_array[q][2] = parameters[3]; //last name
                    records_array[q][3] = parameters[4]; //disease
                    records_array[q][4] = countries_array[i]; //country
                    records_array[q][5] = parameters[5]; //age
                
                    //Case: ENTER record
                    if(parameters[1]=="ENTER")
                    {
                        records_array[q][6] = date_files[i][k]; //entry date
                        records_array[q][7] = "--"; //exit date
                    }
                    //Case: EXIT record
                    else if(parameters[1]=="EXIT")
                    {
                        records_array[q][6] = "--"; //entry date
                        records_array[q][7] = date_files[i][k]; //exit date
                    }
                    //Case: Error so fill record with "--".
                    else
                    {
                        records_array[q][0] = "--";
                        cerr << "ERROR" << endl;
                    }
                }
                //Case: This half-Record hasn't the appropriate number of parameters,so reject it.
                else
                {
                    cerr << "ERROR" << endl;
                }
                q++;
            }
            //Close each date file so as to continue with next.
            newfile.close();
        } 
        
        //Now Worker is ready to manipulate half records who holds them in records_array so as to create a full record...
        for(q=0;q<date_files_per_dir*records_per_file;q++)
        {
            //Case: ENTER record.
            if(records_array[q][6]!="--")
            {
                //Loop through array to find EXIT record with same rID.
                for(j=0;j<date_files_per_dir*records_per_file;j++)
                {
                    //If ENTER record and EXIT record have the rID continue to update exit_date of our full record.
                    if(records_array[q][0]==records_array[j][0] && records_array[j][7]!="--" && records_array[j][0]!="--")
                    {
                        records_array[q][7]=records_array[j][7];
                        records_array[j][0]="--";
                        break;
                    }
                }
            }
        }
      
        //So here we will take from records_array the active full records and insert them to our structures. 
        for(q=0;q<date_files_per_dir*records_per_file;q++)
        {
            //If record is active...
            if(records_array[q][0]!="--")
            {
                //Check if Record is Valid.
                err = Is_Valid(&head,records_array[q][0],records_array[q][1],records_array[q][2],records_array[q][3],records_array[q][4],records_array[q][5],records_array[q][6],records_array[q][7]);

                //Case: Found a problem so reject the record.
                if(CALL_ERROR(err)==1)
                {
                    cerr << "ERROR" << endl;
                } 
                //Record is Valid, so keep on...
                else
                {
                    RecordNode* record = RL_Insert(&head,records_array[q][0],records_array[q][1],records_array[q][2],records_array[q][3],records_array[q][4],records_array[q][5],records_array[q][6],records_array[q][7]);

                    disease_bucket = hash_function2(records_array[q][3],diseases);
                    country_bucket = hash_function2(records_array[q][4],num_of_dirs);

                    //For diseases...
                    //First time in this bucket.
                    if(diseaseHashTable[disease_bucket]==NULL)
                    {    
                        diseaseHashTable[disease_bucket] = CreateBucketNode(bucket_size);
                        InsertBucketEntry(diseaseHashTable[disease_bucket],records_array[q][3],records_array[q][6],bucket_size,record);
                    }
                    //Case: We have already some entries in this bucket...
                    else
                    {
                        InsertBucketEntry(diseaseHashTable[disease_bucket],records_array[q][3],records_array[q][6],bucket_size,record);
                    }
                    
                    //For countries...
                    //First time in this bucket.
                    if(countryHashTable[country_bucket]==NULL)
                    {    
                        countryHashTable[country_bucket] = CreateBucketNode(bucket_size);
                        InsertBucketEntry(countryHashTable[country_bucket],records_array[q][4],records_array[q][6],bucket_size,record);
                    }
                    //Case: We have already some entries in this bucket...
                    else
                    {
                        InsertBucketEntry(countryHashTable[country_bucket],records_array[q][4],records_array[q][6],bucket_size,record);
                    }  
                }
            }
        }

        //Deallocate memory for string arrays
        for(z=0;z<date_files_per_dir*records_per_file;z++)  delete[] records_array[z];
    }

    //Worker has now stored records to our structures and he's ready to create Summary Statistics.
    BucketNode* temp_bucket;
    string* summary_stats = new string();
    *summary_stats="";

    for(i=0;i<num_of_dirs;i++)
    {
        temp_bucket = countryHashTable[i];
        if(temp_bucket!=NULL)
        {
            while(temp_bucket!=NULL)
            {
                for(j=0;j<temp_bucket->counter;j++)
                {
                    Summary_Stats_Of_Dir(temp_bucket->array[j]->root,summary_stats,temp_bucket->array[j]->name);
                }
                temp_bucket = temp_bucket->next;
            }
        }
    }
    
    //Worker passes Summary Stats to Father.
    Worker_Write(CPfifo,*summary_stats,buffersize);
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////Worker is ready to accept requests from father//////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    int SUCCESS=0,FAIL=0;
    
    //Sigaction for SIGINT signal...
    struct sigaction act1;
	memset (&act1, 0, sizeof(act1)); 
	act1.sa_sigaction = sigintorquithandler;
 	act1.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGINT, &act1, NULL) < 0) 
	{
		perror("Worker SIGINT ERROR!");
		return 1;
	}

    //Sigaction for SIGQUIT signal...
    struct sigaction act2;
	memset (&act2, 0, sizeof(act2)); 
	act2.sa_sigaction = sigintorquithandler;
    
 	act2.sa_flags = SA_SIGINFO;
	if (sigaction(SIGQUIT, &act2, NULL) < 0) 
	{
		perror("Worker SIGQUIT ERROR!");
		return 1;
	}

    //Sigaction for SIGUSR1 signal...
    struct sigaction act3;
	memset (&act3, 0, sizeof(act3)); 
	act3.sa_sigaction = sigusr1handler;
 	act3.sa_flags = SA_RESTART | SA_SIGINFO;
	if (sigaction(SIGUSR1, &act3, NULL) < 0) 
	{
		perror("Worker SIGUSR1 ERROR!");
		return 1;
	}

    //Worker is starting get requests from Father...
    while(1)
    {
        stringstream rt(Worker_Read(PCfifo,buffersize));
        string words[9];
        i=0;

        while(rt >> words[i]) i++;   

        //Case: Worker caught signal SIGUSR1, this means that new date files have been added to his dirs.
        if(sigusr1)
        {
            WorkerHasNewDateFiles(num_of_dirs,&countries_array,input_dir,&latest_dates,&head,diseases,diseaseHashTable,countryHashTable);
            sigusr1=false;
        }

        //Case: End of program.
        if(words[0]=="/rightexit") 
        {
            break;
        }
        else if(words[0]=="/listCountries")
        {
            for(k=0;k<num_of_dirs;k++)
            {
                message.append(countries_array[k]);
                message.append(" ");
                message.append(to_string(getpid()));
                message.append("\n");
            }
            Worker_Write(CPfifo,message,buffersize);
            message="";
            SUCCESS++;
        }
        else if(words[0]=="/diseaseFrequency")
        {
            string* disease_message = new string();
            (*disease_message)="";
            //Case: diseaseFrequency without country's filter.
            if(i==4)
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0)
                {    
                    DiseaseFrequency(diseaseHashTable,diseases,words[1],words[2],words[3],"empty",disease_message);
                    Worker_Write(CPfifo,*disease_message,buffersize);
                    if(*disease_message=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
            //Case: diseaseFrequency with country's filter.
            else
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0 && CALL_ERROR(Check_Country(words[4]))==0)
                {
                    DiseaseFrequency(diseaseHashTable,diseases,words[1],words[2],words[3],words[4],disease_message);
                    Worker_Write(CPfifo,*disease_message,buffersize);
                    if(*disease_message=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
            delete disease_message;
        }
        else if(words[0]=="/searchPatientRecord")
        {
            RecordNode* temp_node = head;
            bool flag=false;
            if(CALL_ERROR(Check_Record(words[1]))==0)
            {
                while(temp_node!=NULL)
                {
                    if(temp_node->recordID==words[1]) 
                    {
                        string patient="";
                        patient.append(temp_node->recordID);
                        patient.append(" ");
                        patient.append(temp_node->patientFirstName);
                        patient.append(" ");
                        patient.append(temp_node->patientLastName);
                        patient.append(" ");
                        patient.append(temp_node->diseaseID);
                        patient.append(" ");
                        patient.append(temp_node->age);
                        patient.append(" ");
                        patient.append(temp_node->entryDate);
                        patient.append(" ");
                        patient.append(temp_node->exitDate);
                        Worker_Write(CPfifo,patient,buffersize);
                        flag=true;
                        SUCCESS++;
                        break;
                    }
                    temp_node=temp_node->next;
                }
                if(!flag)   
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
            else
            {
                Worker_Write(CPfifo,"ERROR",buffersize);
                FAIL++;
            }
            
        }
        else if(words[0]=="/topk-AgeRanges")
        {
            if(CALL_ERROR(Check_Dates(words[4],words[5]))==0 && CALL_ERROR(Check_Virus(words[3]))==0 && CALL_ERROR(Check_Country(words[2]))==0 && CALL_ERROR(Check_k(words[1]))==0)
            {
                string topk = TopkAgeRanges(countryHashTable,num_of_dirs,words[2],words[3],words[4],words[5],stoi(words[1]));
                Worker_Write(CPfifo,topk,buffersize);
                if(topk=="ERROR")   FAIL++;
                else    SUCCESS++;
            }
            else
            {
                Worker_Write(CPfifo,"ERROR",buffersize);
                FAIL++;
            }
        }
        else if(words[0]=="/numPatientAdmissions")
        {
            string response;
            //Case: numPatientAdmissions without country's filter.
            if(i==4)
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0)
                {    
                    response = NumPatientsAdmissionsDischarges(diseaseHashTable,diseases,words[1],words[2],words[3],"empty",true);
                    Worker_Write(CPfifo,response,buffersize);
                    if(response=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
            //Case: numPatientAdmissions with country's filter.
            else
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0 && CALL_ERROR(Check_Country(words[4]))==0)
                {
                    response = NumPatientsAdmissionsDischarges(diseaseHashTable,diseases,words[1],words[2],words[3],words[4],true);
                    Worker_Write(CPfifo,response,buffersize);
                    if(response=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
        }
        else if(words[0]=="/numPatientDischarges")
        {
            string dis_response;
            //Case: numPatientAdmissions without country's filter.
            if(i==4)
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0)
                {    
                    dis_response = NumPatientsAdmissionsDischarges(diseaseHashTable,diseases,words[1],words[2],words[3],"empty",false);
                    Worker_Write(CPfifo,dis_response,buffersize);
                    if(dis_response=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
            //Case: numPatientAdmissions with country's filter.
            else
            {
                if(CALL_ERROR(Check_Dates(words[2],words[3]))==0 && CALL_ERROR(Check_Virus(words[1]))==0 && CALL_ERROR(Check_Country(words[4]))==0)
                {
                    dis_response = NumPatientsAdmissionsDischarges(diseaseHashTable,diseases,words[1],words[2],words[3],words[4],false);
                    Worker_Write(CPfifo,dis_response,buffersize);
                    if(dis_response=="ERROR")   FAIL++;
                    else    SUCCESS++;
                }
                else
                {
                    Worker_Write(CPfifo,"ERROR",buffersize);
                    FAIL++;
                }
            }
        }
        else if(words[0]=="/print")
        {
            Print_Hash_Tables(diseases,num_of_dirs,diseaseHashTable,countryHashTable,false);
        } 
        else if(words[0]=="/printwithtree")
        {
            Print_Hash_Tables(diseases,num_of_dirs,diseaseHashTable,countryHashTable,true);
        }
        else if(words[0]=="/print_length")
        {
            cout << "Total Records = " << RL_Length(head) << endl;  
        }  
        else if(words[0]=="/print_records")
        {
            cout << "Total Records = " << RL_Length(head) << endl;  
            RL_Print(head);
        }
        
        if(sigintorquit)
        {
            ofstream myfile;
            string process = "log_files/log_file.";
            process.append(to_string(getpid()));
            myfile.open(process);
            for(i=0;i<num_of_dirs;i++)
            {
                myfile << countries_array[i];
                myfile << "\n";
            }
            myfile << "Total ";
            myfile << to_string((SUCCESS+FAIL));
            myfile << "\n";
            myfile << "SUCCESS ";
            myfile << to_string((SUCCESS));
            myfile << "\n";
            myfile << "FAIL ";
            myfile << to_string((FAIL));
            myfile << "\n";
            myfile.close();
            sigintorquit=false;
            break;
        }
        
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////End of program,after is deallocation of memory//////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    for(i=0;i<num_of_dirs;i++)
    {
        delete[] countries_array[i];
        delete[] date_files[i];
    }
    delete[] countries_array;
    delete[] date_files;
    delete[] latest_dates;
    delete summary_stats;
    delete[] records_array;
    delete_list(head_of_diseases);
    free(whole_message);

    Destroy_RList(&head);
    for(j=0;j<diseases;j++) Destroy_BList(&diseaseHashTable[j]);
    for(j=0;j<num_of_dirs;j++) Destroy_BList(&countryHashTable[j]); 

    return 0;
}