#include <boost/beast/core/detail/base64.hpp> 
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read_until.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace std; 
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;


ssl::stream<tcp::socket>* pStream;
boost::asio::streambuf buf;
ostream reqStream(&buf);

size_t PrintResponse() {
    size_t len = boost::asio::read_until(*pStream, buf, "\r\n");
    istream respStream(&buf);
    string response;
    getline(respStream, response);
    cout << response << endl;
    
    // Извлекаем код ответа сервера (первые три символа строки)
    if (response.size() >= 3) {
        string code = response.substr(0, 3);
        return stoi(code);  // Возвращаем числовой код ответа
    }
    
    return 0;  // Если код не удалось извлечь
}

size_t SendRequest(const string& s) {
    buf.consume(buf.size());  
    reqStream << s << "\r\n";
    return boost::asio::write(*pStream, buf);
}

string base64_encode(const string& input) {
    vector<uint8_t> output((input.size() + 2) / 3 * 4); 
    auto len = boost::beast::detail::base64::encode(output.data(), input.c_str(), input.size());
    return string(output.data(), output.data() + len);
}

int main() {
    system("chcp 65001 > nul"); 
    try {
        auto const host = "smtp.gmail.com";
        auto const port = "465"; 

        boost::asio::io_context ioc;
        ssl::context ctx{ ssl::context::sslv23_client };

        ctx.set_verify_mode(ssl::verify_none);

        tcp::resolver resolver{ ioc };
        ssl::stream<tcp::socket> stream{ ioc, ctx };
        pStream = &stream;

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host)) {
            throw runtime_error("Не удалось установить имя хоста.");
        }

        auto const results = resolver.resolve(host, port);
        boost::asio::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);

        string name, passw, to;
        cout << "Введите адрес отправителя (e-mail): ";
        getline(cin, name);
        cout << "Введите пароль приложения: ";
        getline(cin, passw);
        cout << "Введите адрес получателя (e-mail): ";
        getline(cin, to);

        string encodedName = base64_encode(name);
        string encodedPass = base64_encode(passw);

    
        if (PrintResponse() != 220) {
            cerr << "Ошибка: неверный код ответа при подключении" << endl;
            return EXIT_FAILURE;
        }

        
        SendRequest("EHLO localhost");
        if (PrintResponse() != 250) {
            cerr << "Ошибка: EHLO не принят" << endl;
            return EXIT_FAILURE;
        }

    
        SendRequest("AUTH LOGIN");
        if (PrintResponse() != 334) {
            cerr << "Ошибка: AUTH LOGIN не принят" << endl;
            return EXIT_FAILURE;
        }


        SendRequest(encodedName);
        if (PrintResponse() != 334) {
            cerr << "Ошибка: Логин не принят" << endl;
            return EXIT_FAILURE;
        }

    
        SendRequest(encodedPass);
        if (PrintResponse() != 235) {
            cerr << "Ошибка: Аутентификация не удалась" << endl;
            return EXIT_FAILURE;
        }

       
        SendRequest("MAIL FROM:<" + name + ">");
        if (PrintResponse() != 250) {
            cerr << "Ошибка: MAIL FROM не принят" << endl;
            return EXIT_FAILURE;
        }

      
        SendRequest("RCPT TO:<" + to + ">");
        if (PrintResponse() != 250) {
            cerr << "Ошибка: RCPT TO не принят" << endl;
            return EXIT_FAILURE;
        }

        
        SendRequest("DATA");
        if (PrintResponse() != 354) {
            cerr << "Ошибка: DATA не принято" << endl;
            return EXIT_FAILURE;
        }

       
        SendRequest("From: <" + name + ">");
        SendRequest("To: <" + to + ">");
        string subject;
        cout << "Введите заголовок сообщения: ";
        getline(cin, subject);
        SendRequest("Subject: " + subject);
        SendRequest("Content-Type: text/plain; charset=UTF-8");
        SendRequest("Content-Transfer-Encoding: 8bit");
        SendRequest("");  

        string message;
        cout << "Введите текст сообщения: ";
        getline(cin, message);
        SendRequest(message);
        SendRequest(".");  

        
        if (PrintResponse() != 250) {
            cerr << "Ошибка: Сообщение не отправлено" << endl;
            return EXIT_FAILURE;
        }

        
        SendRequest("QUIT");
        if (PrintResponse() != 221) {
            cerr << "Ошибка: Завершение соединения не удалось" << endl;
            return EXIT_FAILURE;
        }

        cout << "Сообщение успешно отправлено!" << endl;

        try {
            boost::system::error_code ec;
            stream.next_layer().shutdown(tcp::socket::shutdown_both, ec);

            if (ec == boost::asio::error::eof) {
                ec.clear();
            } else if (ec && ec != boost::asio::ssl::error::stream_truncated) {
                throw boost::system::system_error{ec};
            }

            stream.next_layer().close(ec);

            if (ec) {
                cerr << "Ошибка при закрытии сокета: " << ec.message() << endl;
            }

        } catch (const exception& e) {
            cerr << "Ошибка при завершении соединения: " << e.what() << endl;
        }

    } catch (exception const& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
