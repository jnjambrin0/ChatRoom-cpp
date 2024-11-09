#include "utils.h"
#include <string>
#include <iostream>
#include <string>

using namespace std;

//Funcion para recibir en paralelo mensajes reenviados por el servidor
//Recibe por parametros el ID del servidor para recibir datos, y una variable compartida
//"salir"

void recibeMensajesClientes(int serverId, bool& salir){
    vector<unsigned char> buffer; //Buffer de datos que se recibio desde el cliente.
    //Desempaquetaremos todas las variables que empaqueto el cliente en el mismo orden

    string nombreUsuario;//Variable para almacenar el nombre del usuario que escribio el mensaje

    string mensaje; //Variable para almacenar el mensaje bucle mientras no salir
    while (!salir) {
        //Recibir mensaje del servidor
        recvMSG(serverId, buffer);
        //si hay datos en el buffer:
        if (buffer.size() > 0) {
            // - desempaquetar del buffer la longitud del nombreUsuario
            // - redimensionar variable nombreUsuario con esa longitud, para poder almacenar el nombreUsuario
            nombreUsuario.resize(unpack<int>(buffer));
            unpackv<char>(buffer, (char*)nombreUsuario.data(), nombreUsuario.size());

            // - desempaquetar del buffer la longitud del mensaje
            // - redimensionar variable mensaje con esa longitud, para poder almacenar el mensaje
            mensaje.resize(unpack<int>(buffer));
            unpackv<char>(buffer, (char*)mensaje.data(), mensaje.size());

            if (mensaje == "[!] El servidor te está desconectando") {
                cout << mensaje << endl;
                salir = true;
            }else {
                //mostrar mensaje recibido
                cout << "[-] Mensaje recibido:"<<nombreUsuario<<"  dice:"<<mensaje<< endl;
            }
            buffer.clear();
        }
        else {
        //conexion cerrada, salir.
            salir = true;
            usleep(100);
        }
    }
}

int main(int argc, char** argv){

    //Empaquetaremos todas las variables que queremos enviar dentro de este buffer para realizar un único envío.
    vector<unsigned char> buffer; //Buffer de datos que se enviará al server.

    string nombreUsuario;// Nombre del usuario que entra al chat
    string mensaje;// Mensaje que escribe el usuario
    string ip_servidor;// IP del servidor al que se conectará el cliente
    bool salir = false;

    //Pedir nombre de usuario por terminal
    cout << "[*] Introduzca nombre de usuario:" << endl;
    getline(cin, nombreUsuario);

    // Pedir la ip del servidor
    cout << "[*] Introduzca ip del servidor (poner 'default' para coger la ip por defecto):" << endl;
    getline(cin, ip_servidor);

    if (ip_servidor == "default") {
        ip_servidor = "44.192.29.220";
    }
    

    //Iniciar conexión al server en ip_servidor:3000
    auto conn = initClient(ip_servidor, 3000);

    //Iniciar thread "recibeMensajesClientes". Debe pasarse por parámetros las variables "conn.serverId" y una referencia compartida a "salir"
    thread* th = new thread(recibeMensajesClientes, conn.serverId, ref(salir));

    //crear buffer de envío que contenga datos de nombre de usuario y texto
    //empaquetar tamaño de nombreUsuario
    pack(buffer, (int)nombreUsuario.size());  
    //empaquetar datos de nombreUsuario
    packv(buffer, (char*)nombreUsuario.data(), nombreUsuario.size());
    //Enviar buffer al servidor
    sendMSG(conn.serverId, buffer);

    //IMPORTANTE limpiar buffer una vez usado:
    buffer.clear();

    //Bucle hasta que el usuario escribe "exit()"
    do {
    // Leer mensaje de texto del usuario
        getline(cin, mensaje);

        bool privado = false;
        string usuario_privado;

        if (mensaje == "/privado"){
            privado = true;

            cout << "[*] Nombre del usuario para el mensaje privado:" << endl;
            getline(cin, usuario_privado);
            cout << "  - Introduzca el mensaje privado: ";
            getline(cin, mensaje);
        }

        // Empaquetamos la flag de mensaje privado
        pack(buffer, (int)privado);

        if (privado){
            // Empaquetamos el tamaño y datos de usuario_privado
            pack(buffer, (int)usuario_privado.size());
            packv(buffer, (char*)usuario_privado.data(), usuario_privado.size());
        }

        // Empaquetamos el tamaño y datos de nombreUsuario
        pack(buffer, (int)nombreUsuario.size());
        packv(buffer, (char*)nombreUsuario.data(), nombreUsuario.size());

        // Empaquetamos el tamaño y datos del mensaje
        pack(buffer, (int)mensaje.size());
        packv(buffer, (char*)mensaje.data(), mensaje.size());

        // Enviamos el buffer al servidor
        sendMSG(conn.serverId, buffer);

        // Limpiamos el buffer
        buffer.clear();

    } while (mensaje != "exit()");

    //Marcar variable "salir" para detener el hilo de recepción de mensajes

    salir = true;
    th->join(); //sincronizar con el thread antes de cerrar la conexión
    
    //Cerrar conexión con el servidor
    closeConnection(conn.serverId);
    
    return 0;
}