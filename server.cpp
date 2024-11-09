#include "utils.h"
#include <iostream>
#include <string>
#include <thread>
#include <list>
#include <algorithm>

using namespace std;

//Función para recibir en paralelo mensajes de un cliente que inició conexión con el servidor
//Recibe por parámetros el ID del cliente para recibir datos, y una variable compartida "users"
//La variable "users" contendrá la lista de usuarios que se han conectado hasta ahora, y se usará
//para reenviar los mensajes

// Estructura usuario para poder mapear nombre usuario e id usuario
struct User{
    string nombre;
    int id;
};

void atiendeConexion(int clientId, list<User>& usersList){

    vector<unsigned char> buffer; //Buffer de datos que se recibirán desde el cliente.
                                  //Desempaquetaremos todas las variables que
                                  //empaquetó el cliente en el mismo orden

    string nombreUsuario; // Nombre de usuario
    string usuario_privado; // Nombre de usuario privado
    string mensaje; // Mensaje de texto enviado por el usuario

    // Recibir nombre de usuario del cliente en el buffer
    recvMSG(clientId, buffer);

    // Desempaquetar nombre de usuario:
    // Para reconstruir un nombre:
    // - desempaquetar del buffer la longitud del nombre
    // - redimensionar variable nombreUsuario con esa longitud, para poder almacenar el nombre
    nombreUsuario.resize(unpack<int>(buffer));
    // - desempaquetar el texto del nombre en los datos de la variable nombreUsuario
    unpackv<char>(buffer, (char*)nombreUsuario.data(), nombreUsuario.size());
    buffer.clear();

    cout << "[+] Usuario Conectado: " << nombreUsuario << endl;

    // Añadir usuario a la lista
    User newUser = {nombreUsuario, clientId};
    usersList.push_back(newUser);

    //Bucle repetir mientras no reciba mensaje "exit()" del cliente.
    //El mensaje "exit()" se usará como señal de salida del cliente, en ese momento se cierra conexión
    //con él y acaba este thread
    do{
        //Recibir mensaje de texto del cliente
        recvMSG(clientId, buffer);

        //si hay datos en el buffer, recibir
        if (buffer.size() > 0) {
            // Comprobamos si el mensaje es privado o no
            bool privado = unpack<int>(buffer);

            if (privado){
                // Si es privado desempaquetamos el nombre del usuario a enviar el mensaje privado
                usuario_privado.resize(unpack<int>(buffer));
                unpackv<char>(buffer, (char*)usuario_privado.data(), usuario_privado.size());
            }

            // Desempaquetar nombre del usuario
            nombreUsuario.resize(unpack<int>(buffer));
            unpackv<char>(buffer, (char*)nombreUsuario.data(), nombreUsuario.size());

            // Desempaquetar mensaje
            mensaje.resize(unpack<int>(buffer));
            unpackv<char>(buffer, (char*)mensaje.data(), mensaje.size());

            // Mostrar mensaje recibido
            cout << "Mensaje recibido de " << nombreUsuario << ": " << mensaje << endl;

            if (mensaje == "exit()"){
                // Enviar mensaje de desconexión al cliente
                vector<unsigned char> buffer_salida;
                string mensajeSalida = "El servidor te está desconectando";
                string nombreServidor = "Servidor";

                pack(buffer_salida, (int)nombreServidor.size());
                packv(buffer_salida, (char*)nombreServidor.data(), nombreServidor.size());

                pack(buffer_salida, (int)mensajeSalida.size());
                packv(buffer_salida, (char*)mensajeSalida.data(), mensajeSalida.size());

                sendMSG(clientId, buffer_salida);
                buffer_salida.clear();
                break;
            }

            if (privado){
                // Buscar usuario del mensaje privado en la lista de usuarios
                // Para hacer esta funcion como el if de abajo he buscado 
                // informacion en internet ya que no sabia bien como hacerlo
                auto it = find_if(usersList.begin(), usersList.end(), [&](const User& user) {
                    return user.nombre == usuario_privado;
                });

                if (it != usersList.end()){
                    int clienteIdDest = it->id;

                    vector<unsigned char> buffer_privado;

                    // Empaquetamos el nombre del usuario privado
                    pack(buffer_privado, (int)nombreUsuario.size());
                    packv(buffer_privado, (char*)nombreUsuario.data(), nombreUsuario.size());

                    // Empaquetamos el mensaje
                    pack(buffer_privado, (int)mensaje.size());
                    packv(buffer_privado, (char*)mensaje.data(), mensaje.size());

                    // Enviamos el mensaje privado al usuario privado
                    sendMSG(clienteIdDest, buffer_privado);

                    buffer_privado.clear();
                } else {
                    // Error si el usuario privado no existe 
                    cout << "[!] ERROR: Usuario " << usuario_privado << " no encontrado." << endl;
                    vector<unsigned char> buffer_error;

                    string mensaje_error = "[!] ERROR: Usuario no encontrado";
                    string nombre_servidor = "Servidor";

                    // Empaquetamos el nombre del servidor
                    pack(buffer_error, (int)nombre_servidor.size());
                    packv(buffer_error, (char*)nombre_servidor.data(), nombre_servidor.size());

                    // Empaquetamos el mensaje de error
                    pack(buffer_error, (int)mensaje_error.size());
                    packv(buffer_error, (char*)mensaje_error.data(), mensaje_error.size());

                    // Enviamos el mensaje de error al usuario que intentó enviar el mensaje privado
                    sendMSG(clientId, buffer_error);
                    buffer_error.clear();
                }
            } else {
                // Mensaje público: enviar a todos los usuarios excepto al usuario que lo envia
                vector<unsigned char> buffer_envio;

                // Empaquetamos el nombre del usuario
                pack(buffer_envio, (int)nombreUsuario.size());
                packv(buffer_envio, (char*)nombreUsuario.data(), nombreUsuario.size());

                // Empaquetamos el mensaje
                pack(buffer_envio, (int)mensaje.size());
                packv(buffer_envio, (char*)mensaje.data(), mensaje.size());

                for (const auto& user : usersList){
                    if (user.id != clientId){
                        sendMSG(user.id, buffer_envio);
                    }
                }

                buffer_envio.clear();
            }

            buffer.clear();

        } else {
            // Error de conexión con el cliente
            cerr << "[!] Error de conexión con el cliente" << endl;
            break;
        }

    } while (true);

    // Eliminamos usuario de la lista cuanod se desconecta
    auto it = find_if(usersList.begin(), usersList.end(), [&](const User& user) {
        return user.id == clientId;
    });

    if (it != usersList.end()) {
        usersList.erase(it);
    }

    // Cerrar conexión con el cliente
    closeConnection(clientId);
}


int main(int argc, char** argv){

    auto conn = initServer(3000);//Iniciar el server en el puerto 3000

    list<User> usersList;
    while (1) //bucle infinito
    {
        //Esperar conexión de un cliente
        while (!checkClient()) usleep(100);
        //Una vez conectado, conseguir su identificador (puede haber varios clientes)
        auto clientId = getLastClientID();
        //Crear hilo paralelo en el que se ejecuta "atiendeConexion"
        //recibe el identificador de cliente y una referencia a la lista compartida de usuarios
        thread* th = new thread(atiendeConexion, clientId, ref(usersList));
    }
    close(conn); //cerrar el servidor
    return 0;
}