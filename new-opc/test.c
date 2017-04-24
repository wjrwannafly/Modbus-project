#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/tcp.h>
#include "open62541.h"
#include <sys/time.h>
#include <time.h>
#define WRONG 2
#define input 512
#define output 768
#define analog 640
#define opc_port 16664
uint16_t tab_inp[1000]={0};
uint16_t tab_oup[1000]={0};
uint16_t tab_ana[1000]={0};
int *sockfd;
UA_Server *server;
int data_r;
typedef struct _DATA_SOURCE{
	char* name;
	unsigned char type;   // 1 stand for DI, 2 stand for DO ,3 stand for analoy
	int state;			  // I\O口状态，高1，低0
	unsigned char H8;
	unsigned char L8;
}DATA_SOURCE;

DATA_SOURCE source[] = {
	{"DI000", 1,0,0,0},
	{"DI001", 1,0,0,0},
	{"DI002", 1,0,0,0},
	{"DI003", 1,0,0,0},
	{"DI004", 1,0,0,0},
	{"DI005", 1,0,0,0},
	{"DI006", 1,0,0,0},
	{"DI007", 1,0,0,0},
	{"DI008", 1,0,0,0},
	{"DI009", 1,0,0,0},
	{"DI010", 1,0,0,0},
	{"DI011", 1,0,0,0},
	{"DI012", 1,0,0,0},
	{"DI013", 1,0,0,0},
	{"DI014", 1,0,0,0},
	{"DI015", 1,0,0,0},
	{"DO000", 2,0,0,0},
	{"DO001", 2,0,0,0},
	{"DO002", 2,0,0,0},
	{"DO003", 2,0,0,0},
	{"DO004", 2,0,0,0},
	{"DO005", 2,0,0,0},
	{"DO006", 2,0,0,0},
	{"DO007", 2,0,0,0},
	{"DO008", 2,0,0,0},
	{"DO009", 2,0,0,0},
	{"DO010", 2,0,0,0},
	{"DO011", 2,0,0,0},
	{"DO012", 2,0,0,0},
	{"DO013", 2,0,0,0},
	{"DO014", 2,0,0,0},
	{"DO015", 2,0,0,0},
	{"AI001", 3,0,0,0},
	{"AI002", 3,0,0,0},
	{"AI003", 3,0,0,0},
	{"AI004", 3,0,0,0},
	{"AO001", 4,0,0,0},
	{"AO002", 4,0,0,0},
	{"AO003", 4,0,0,0},
	{"AO004", 4,0,0,0}
};

UA_Boolean running = true;
static void stopHandler(int sig) {
	running = false;
}

int Merge(int a,unsigned char b,unsigned char c)
{
	a = (b << 8) | c;
	return a;

}

/*通过名字读数据*/
void* nodeIdFindData(const UA_NodeId nodeId)
{
	int i;
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++)
	{
		if(strncmp((char*)nodeId.identifier.string.data, source[i].name, strlen(source[i].name)) == 0)
		{
			if(source[i].type == 1||source[i].type ==2) {
				return &source[i].state;
			}
			else if(source[i].type == 3||source[i].type ==4)
				{
					data_r=Merge(0,source[i].H8, source[i].L8);
					return &data_r;
				}
		}
	}
	printf("not find:%s!\n",nodeId.identifier.string.data);
	return NULL;
}

/*通过名字写数据*/
void* nodeIdwriteData(const UA_NodeId nodeId, const UA_Variant *data)
{
	int i;
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++)
	{
		if(strncmp(source[i].name, (char*)nodeId.identifier.string.data, strlen(source[i].name)) == 0)
		{
			if(source[i].type == 1||source[i].type ==2) {
				source[i].state=*(int*)data->data;
				return &source[i].state;
			}
			else if(source[i].type == 3||source[i].type ==4) {
				 	source[i].H8 =*(int*)data->data >> 8;
					source[i].L8 =*(int*)data->data & 0x0ff;
					printf("write 16 hex %x %x\n",source[i].H8,source[i].L8);
				 return &source[i].H8;
			}
		}
	}
	printf("not find:%s!\n",nodeId.identifier.string.data);
	return NULL;
}

/*读数据*/
static UA_StatusCode
readDataSource(void *handle, const UA_NodeId nodeId, UA_Boolean sourceTimeStamp,
             const UA_NumericRange *range, UA_DataValue *dataValue) {
	if(range) {
        dataValue->hasStatus = true;
        dataValue->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
        return UA_STATUSCODE_GOOD;
    }
	UA_Int16 temp;
	if(nodeIdFindData(nodeId) != NULL)
		temp = *(UA_Int16*)nodeIdFindData(nodeId);
	else
		temp = 0;
	dataValue->sourceTimestamp = UA_DateTime_now();
	dataValue->hasValue = true;
	dataValue->hasSourceTimestamp = true;
    UA_Variant_setScalarCopy(&dataValue->value, &temp, &UA_TYPES[UA_TYPES_INT16]);
	printf("Node read %s\n", nodeId.identifier.string.data);
	printf("read Value %d\n", temp);
    return UA_STATUSCODE_GOOD;
}

/*写数据*/
static UA_StatusCode
writeDataSource(void *handle, const UA_NodeId nodeId, const UA_Variant *data,
			const UA_NumericRange *range) {
				UA_Int16 temp;
	if(UA_Variant_isScalar(data) && data->type == &UA_TYPES[UA_TYPES_INT16] && data->data)
	{
		if(nodeIdwriteData(nodeId, data)!=NULL)
			temp = *(UA_Int16*)data->data;
	}
//	*ANALOY->state=*handle;
	printf("Node written %s\n", nodeId.identifier.string.data);
	printf("written value %d\n",  temp);
    return UA_STATUSCODE_GOOD;
}

void add_dataSource_to_opcServer()
{
	int i;
	for(i=0; i<sizeof(source)/sizeof(DATA_SOURCE);i++) {
		if(source[i].type == 1) {
					UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readDataSource,
		.write = NULL};

		UA_VariableAttributes *attr = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr);
//			UA_Int32 intState = (UA_Int32)source[i].state;
//		UA_Variant_setScalar(&attr->value, &intState, &UA_TYPES[UA_TYPES_INT32]);
		attr->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
		attr->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(1, 1000);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
		UA_Server_addDataSourceVariableNode(server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr, dateDataSource, NULL);
		}
		if(source[i].type == 2) {
					UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readDataSource,
		.write = writeDataSource};

		UA_VariableAttributes *attr = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr);
//			UA_Int32 intState = (UA_Int32)source[i].state;
//		UA_Variant_setScalar(&attr->value, &intState, &UA_TYPES[UA_TYPES_INT32]);
		attr->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
		attr->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(1, 2000);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
		UA_Server_addDataSourceVariableNode(server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr, dateDataSource, NULL);
		}
		if(source[i].type == 3) {
					UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readDataSource,
		.write = writeDataSource};

		UA_VariableAttributes *attr = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr);
//			UA_Int32 intData = (UA_Int32)source[i].data;
//		UA_Variant_setScalar(&attr->value, &intData, &UA_TYPES[UA_TYPES_INT32]);
		attr->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
		attr->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(1, 3000);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
		UA_Server_addDataSourceVariableNode(server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr, dateDataSource, NULL);
		}
		if(source[i].type == 4) {
					UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readDataSource,
		.write = writeDataSource};

		UA_VariableAttributes *attr = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr);
//			UA_Int32 intData = (UA_Int32)source[i].data;
//		UA_Variant_setScalar(&attr->value, &intData, &UA_TYPES[UA_TYPES_INT32]);
		attr->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
		attr->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(1, 4000);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
		UA_Server_addDataSourceVariableNode(server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr, dateDataSource, NULL);
		}
	}
}

void handle_opcua_server(void * arg){
		//signal(SIGINT,  stopHandler);
    //signal(SIGTERM, stopHandler);

	UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, opc_port);
	config.networkLayers = &nl;
    config.networkLayersSize = 1;
	server = UA_Server_new(config);


	/* add a variable node Digital to the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    attr.description = UA_LOCALIZEDTEXT("en_US","DI");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","DI");


	/* Add the three variable node to the information model */
    UA_NodeId myIntegerNodeId = UA_NODEID_NUMERIC(1, 1000);
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "DI");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NULL, attr, NULL, NULL);

    UA_VariableAttributes_init(&attr);
    attr.description = UA_LOCALIZEDTEXT("en_US","DO");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","DO");
    UA_Server_addVariableNode(server,  UA_NODEID_NUMERIC(1, 2000), UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "DO"),
                              UA_NODEID_NULL, attr, NULL, NULL);

    UA_VariableAttributes_init(&attr);
    attr.description = UA_LOCALIZEDTEXT("en_US","AI");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","AI");
    UA_Server_addVariableNode(server,  UA_NODEID_NUMERIC(1, 3000), UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "AI"),
                              UA_NODEID_NULL, attr, NULL, NULL);

	 UA_VariableAttributes_init(&attr);
    attr.description = UA_LOCALIZEDTEXT("en_US","AO");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","AO");
    UA_Server_addVariableNode(server,  UA_NODEID_NUMERIC(1, 4000), UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "AO"),
                              UA_NODEID_NULL, attr, NULL, NULL);


	add_dataSource_to_opcServer();


    UA_Server_run(server, &running);
	UA_Server_delete(server);
    nl.deleteMembers(&nl);
}

void Initialization(int *sockfd, struct sockaddr_in *local){
    int err;
    sockfd=malloc(sizeof(int)*10);
    *sockfd=socket(AF_INET,SOCK_STREAM, 0);
    if(*sockfd < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
    memset(local, 0, sizeof(struct sockaddr_in));
    local->sin_family = AF_INET;
	local->sin_addr.s_addr= inet_addr("127.0.0.1");
	local->sin_port = htons(6666);//重新设置local的值,并转换格式
	if(bind(*sockfd, (struct sockaddr*)local, sizeof(struct sockaddr_in))<0);
		printf("Socket has been created.\n");

	err = connect(*sockfd,(struct sockaddr*)local, sizeof(struct sockaddr_in));//connect with server.
	if(err < 0){
		perror("connect");
		exit(EXIT_FAILURE);
	}
}
/* 写寄存器函数,a 为要写的值,b为寄存器的地址。*/
void SendData(int a,int b){
    int err,e,f;
    e = b >> 8;
	f = b & 0x0ff;
    if(a==1)
    {
    int buffer[] = {0x06, 0x06, e,f, 0xAA, 0x00,0x00};
    send(*sockfd,buffer,sizeof(buffer)+1,0);
        if(err < 0)
        {
        perror("send");
        exit(EXIT_FAILURE);
        }
    }
    if(a==0)
    {
    int buffer[] = {0x00, 0x06, e, f, 0x55, 0x00,0x00};
    send(*sockfd,buffer,sizeof(buffer)+1,0);
        if(err < 0)
        {
        perror("send");
        exit(EXIT_FAILURE);
        }
    }
}
void ReadData()
{   int err,i,e,f;
    while(1)
    {
        i=0;
        for(i=0;i<16;i++)
        {   e = (input+i) >> 8;
            f = (input+i) & 0x0ff;
            int buffer[] = {0x00, 0x04, e,f,0x00,0x00};
            int buffer1[5];
            send(*sockfd,buffer,sizeof(buffer)+1,0);
            recv(*sockfd,buffer1,sizeof((buffer1)+1),0);
            if(buffer1[2]=170)
            {
                source[i].state=1;
            }
            else if(buffer1[2]=85)
            {
                source[i].state=0;
            }
            else{source[i].state=WRONG;}

        }
    }
}
void ReadDataDO()
{   int err,i,e,f;
        i=0;
        for(i=0;i<16;i++)
        {   e = (output+i) >> 8;
            f = (output+i) & 0x0ff;
            int buffer[] = {0x06, 0x04, e,f,0x00,0x00};
            int buffer1[5];
            send(*sockfd,buffer,sizeof(buffer)+1,0);
            recv(*sockfd,buffer1,sizeof((buffer1)+1),0);
            if(buffer1[2]=170)
            {
                source[i+16].state=1;
            }
            else if(buffer1[2]=85)
            {
                source[i+16].state=0;
            }
            else{source[i+16].state=WRONG;}

        }
}
void Write()
{
    int num[16]={0},i=0;
    ReadDataDO();
    for(i=0;i<16;i++)
    {
        num[i]=source[i+16].state;
    }
    while(1)
    {
        for(i=0;i<16;i++)
        {
            if(source[i+16].state!=num[i])
            {
                SendData(source[i+16].state,output+i);
                num[i]=source[i+16].state;
            }
        }
    }

}

#if 1
int main()
{
        struct sockaddr_in *local;
        local=malloc(sizeof(int)*100);
        sockfd=malloc(sizeof(int)*10);
		int ret,ret1,ret2;
		pthread_t read;
		pthread_t write;
		pthread_t opcua_server_id;
		Initialization(sockfd,local);
        ret=pthread_create(&read,NULL,(void*)ReadData,NULL);
        ret1=pthread_create(&write,NULL,(void*)Write,NULL);
		ret2=pthread_create(&opcua_server_id,NULL,(void *)handle_opcua_server,NULL);
		if(ret!=0||ret1!=0||ret2!=0)
        {
		printf("Create pthread error!\n");
		exit(1);
        }
        while(1);
        return 0;

}
#endif

#if 0
int main()
{
    Initialization();
    Modbus_read_DI();
    printf("%d",source[0].state);
}
#endif // 1