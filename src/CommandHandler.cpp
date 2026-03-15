#include "CommandHandler.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "SettingsManager.h"
#include "StringUtils.h"
#include "InputEmulator.h"
#include "Logger.h"
#include <sstream>

void CommandHandler::HandleData(int id, const std::string& data, WSServerThread& srv) {
    try {
        std::stringstream ss(data);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        const std::string command = pt.get<std::string>("command", "");
        const std::string payload = pt.get<std::string>("data", "");

        if (command == "auth") {
            if (payload == srv.getPasskey()) {
                srv.setAuthenticated(id, true);
                Logger::PostLogMessage("Клиент " + std::to_string(id) + " успешно авторизован");
                srv.generateNewPasskey();
            } else {
                Logger::PostLogMessage("Клиент " + std::to_string(id) + ": неверный passkey. Соединение разорвано.");
                srv.close(id);
            }
            return;
        }

        if (!srv.isAuthenticated(id)) {
            Logger::PostLogMessage("Клиент " + std::to_string(id) + ": попытка передачи данных без авторизации. Соединение разорвано.");
            srv.close(id);
            return;
        }

        if (command == "scan") {
            Logger::PostLogMessage("Данные от " + std::to_string(id) + ": " + payload);
            const std::wstring wideData = StringUtils::Utf8ToWide(payload);
            const auto& s = SettingsManager::getInstance().getSettings();
            InputEmulator::SendString(wideData, s.prefix, s.postfix1, s.postfix2);
        } else if (command == "heartbeat") {
            // Игнорируем heartbeat без лишнего логирования
        } else {
            Logger::PostLogMessage("Неизвестная команда от " + std::to_string(id) + ": " + command);
        }

    } catch (const std::exception& e) {
        Logger::PostLogMessage("Ошибка разбора JSON от " + std::to_string(id) + ": " + e.what());
        if (!srv.isAuthenticated(id)) {
            srv.close(id);
        }
    }
}
