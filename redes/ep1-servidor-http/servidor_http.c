#include "servidor_http.h"

void executar_servidor_http(int socket){
	int n;
	char linha[MAXLINE + 1];
	socket_conexao = socket;

	if(( n = read(socket_conexao, linha, MAXLINE) ) == 0)
		return;
	linha[n]= '\0';
	
	printf("[%d:\n%s]\n", getpid(), linha);

	requisicao req = interpretar_requisicao(linha);
	
	while(  strcmp(linha, "\r\n") != 0 &&
			strcmp(linha + n - 4, "\r\n\r\n") != 0 &&
			strcmp(linha + n - 2, "\n\n") != 0) {
		n = read(socket_conexao, linha, MAXLINE);
		linha[n] = '\0';
	}

	resposta resp;
	resp.protocolo = HTTP11;
	
	strcpy(resp.arquivo, diretorio_www);
	strcat(resp.arquivo, req.caminho);
	
	struct stat st;
	if(stat(resp.arquivo, &st) == 0 && S_ISREG(st.st_mode))
		resp.status = 200;
	else {
		resp.status = 404;
		getcwd(resp.arquivo, PATH_MAX);
		strcat(resp.arquivo, "/404.html");
	}
	
	enviar_resposta(resp);
	enviar_arquivo(resp.arquivo);
}

requisicao interpretar_requisicao(char linha[]){
	requisicao req;
	char *resto;
	char *token;
	char *ptr = linha;

	req.tipo = REQ_INVALIDA;
	req.protocolo = PROT_INVALIDO;
	req.caminho[0] = '\0';

	token = strtok_r(ptr, " ", &resto);
	if(!token)
		return req;

	if(strcmp(token, "GET") == 0)
		req.tipo = GET;
	else if (strcmp(token, "POST") == 0)
		req.tipo = POST;
	
	ptr = resto;
	token = strtok_r(ptr, " ", &resto);
	if(!token)
		return req;
	
	strncpy(req.caminho, token, MAXLINE);
	req.caminho[MAXLINE] = '\0';
	
	ptr = resto;
	token = strtok_r(ptr, " ", &resto);
	if(!token)
		return req;

	if(strcmp(token, "HTTP/1.0") == 0)
		req.protocolo = HTTP10;
	else if	(strcmp(token, "HTTP/1.0") == 0)
		req.protocolo = HTTP11;
	
	return req;
}

void enviar_resposta(resposta resp){
	char tmp[MAXLINE + 1];
	char *protocolo, *nome_status;
	int tam_tmp;
	
	buffer[0] = '\0';
	tam_buffer = 1;
	
	protocolo = "";
	if(resp.protocolo == HTTP10)
		protocolo = "HTTP/1.0";
	else if(resp.protocolo == HTTP11)
		protocolo = "HTTP/1.1";
	
	nome_status = "";
	if(resp.status == 200)
		nome_status = "OK";
	else if(resp.status == 404)
		nome_status = "Not found";

	//HTTP/1.1 200 OK	
	tam_tmp = snprintf(tmp, sizeof(tmp), "%s %d %s\r\n", protocolo, resp.status, 
						nome_status);
	escrever_buffer(tmp, tam_tmp);
	
	
	//Date:	Sun, 26 Aug 2012 13:12:48 GMT
	time_t agora = time(0);
	struct tm tm = *gmtime(&agora);
	tam_tmp = strftime(tmp, sizeof(tmp), "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", &tm);
	escrever_buffer(tmp, tam_tmp);
	
	//Server: Apache/2.2.16
	tam_tmp = snprintf(tmp, sizeof(tmp), "Server: %s\r\n", NOME_SERVIDOR);
	escrever_buffer(tmp, tam_tmp);
	
	
	struct stat infos;
	int leu_infos_arquivo = stat(resp.arquivo, &infos) == 0 ? 1 : 0;
	if (resp.status == 200 && leu_infos_arquivo) {
		//Last-Modified: Sun, 31 Jul 2011 17:26:08 GMT
		tam_tmp = strftime(tmp, sizeof(tmp), "Last-Modified: %a, %d %b %Y %H:%M:%S %Z\r\n", 
							localtime( &infos.st_mtime));
		escrever_buffer(tmp, tam_tmp);
		
		//ETag:	"21838f9-1ed-4a960cf6e557a"
		tam_tmp = snprintf(tmp, sizeof(tmp), "ETag: \"%d\"\r\n", (int) infos.st_mtime);
		escrever_buffer(tmp, tam_tmp);
		
		//Accept-Ranges:	bytes
		tam_tmp = snprintf(tmp, sizeof(tmp), "Accept-Ranges: bytes\r\n");
		escrever_buffer(tmp, tam_tmp);
	}
	
	//Vary:	Accept-Encoding
	tam_tmp = snprintf(tmp, sizeof(tmp), "Vary: Accept-Encoding\r\n");
	escrever_buffer(tmp, tam_tmp);

	//Content-Encoding:	gzip
	//TODO Implementar gzip e modificar content-length

	//Content-Length:	349
	tam_tmp = snprintf(tmp, sizeof(tmp), "Content-Length: %d\r\n", (int) infos.st_size);
	escrever_buffer(tmp, tam_tmp);
	
	//Keep-Alive: timeout=15, max=100
	//TODO
	
	//Connection: Keep-Alive
	//TODO trocar de close para Keep-Alive
	tam_tmp = snprintf(tmp, sizeof(tmp), "Connection: close\r\n");
	escrever_buffer(tmp, tam_tmp);
	
	//Content-Type: text/html; charset=iso-8859-1
	tam_tmp = snprintf(tmp, sizeof(tmp), "Content-Type: %s\r\n", 
						obter_content_type(resp.arquivo));
	escrever_buffer(tmp, tam_tmp);
	
	escrever_buffer("\r\n", 2);
	enviar_buffer();
}

void enviar_buffer(){
	write(socket_conexao, buffer, tam_buffer - 1);
	buffer[0] = '\0';
	tam_buffer = 1;		
}

void escrever_buffer(char string[], int tam_string){
	if(tam_string + tam_buffer > MAXLINE)
		enviar_buffer();
	
	strcat(buffer, string);
	tam_buffer += tam_string;
}

void enviar_arquivo(char nome_arquivo[]){
	char buffer[MAXLINE];
	FILE *arquivo = fopen(nome_arquivo, "rb");
	
	if(!arquivo){
		fprintf(stderr, "abrir %s :(", nome_arquivo);
		return;
	}
	
	int lido;
	while(!feof(arquivo)){
		lido = fread(buffer, 1, MAXLINE, arquivo);
		write(socket_conexao, buffer, lido);
	}
	
	fclose(arquivo);
}

void definir_diretorio_www(char diretorio[]){
	if(!diretorio){
		getcwd(diretorio_www, PATH_MAX);
		return;
	}
	
	realpath(diretorio, diretorio_www); 
}


char* obter_content_type(char nome_arquivo[]){
	while(*nome_arquivo)
		nome_arquivo++;
	
	nome_arquivo -= 4;
	
	if (strcmp(".htm", nome_arquivo) == 0 || strcmp("html", nome_arquivo) == 0)
		return "text/html";
	if (strcmp(".jpg", nome_arquivo) == 0 || strcmp("jpeg", nome_arquivo) == 0)
		return "image/jpeg";
	if (strcmp(".gif", nome_arquivo) == 0)
		return "image/gif";
	if (strcmp(".txt", nome_arquivo) == 0)
		return "text/plain";
	if (strcmp(".pdf", nome_arquivo) == 0)
		return "application/pdf";
	return "application/octet-stream";
}
