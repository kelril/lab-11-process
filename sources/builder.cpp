#include "builder.hpp"

void build(int argc, char* argv[]) {

    options_description desc("Allowed options");
    desc.add_options()
        ("help", "выводим вспомогательное сообщение")
        ("config", value<std::string>(), "указываем конфигурацию сборки (по умолчанию Debug)")
        ("install", "добавляем этап установки (в директорию _install)")
        ("pack", "добавляем этап упаковки (в архив формата tar.gz)")
        ("timeout", value<time_t>(), "указываем время ожидания (в секундах)")
    ;

    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help") && !vm.count("config")  && !vm.count("pack")
                         && !vm.count("timeout") && !vm.count("install")) {
        std::cout << desc << "\n";
    }
    else {
        std::string config = "Debug";
        time_t timeout = 0;
        time_t time_spent = 0;
        if (vm.count("timeout")) {
            timeout = vm["timeout"].as<time_t>();
        }

        if (vm.count("config")) {
            config = vm["config"].as<std::string>();
        }

        std::string command_1 = "sudo cmake -H. -B_build -DCMAKE_INSTALL_" +
                             std::string("PREFIX=_install -DCMAKE_BUILD_TYPE=");
        std::string command_2 = "sudo cmake --build _build";
        std::string command_3 = "sudo cmake --build _build --target install";
        std::string command_4 = "sudo cmake --build _build --target package";

        if (config == "Debug" || config == "Release") {
            int res_1 = 0;
            int res_2 = 0;
            command_1 += config;

            auto t1 = async::spawn([&res_1, config, timeout, &time_spent,
                                    command_1, command_2] () mutable {
                time_t start_1 = std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now()
                );

                create_child(command_1, timeout);
                time_t end_1 = std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now()
                );

                time_spent += end_1 - start_1;
                std::cerr << "\n" << "time spent -" << time_spent << "\n";
                time_t period_2 = timeout - time_spent;
                create_child(command_2, period_2, res_1);
                time_t end_2 = std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now()
                );

                time_spent += end_2 - end_1;
            });

            if (vm.count("install") && res_1 == 0) {
                auto t2 = t1.then([&res_2, command_3,
                                            timeout, &time_spent] () mutable {

                    time_t period_3 = timeout - time_spent;
                    time_t start_3 = std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now()
                    );
                    create_child(command_3, period_3, res_2);
                    time_t end_3 = std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now()
                    );

                    time_spent += end_3 - start_3;
                });

                if (vm.count("pack") && res_2 == 0) {
                    auto t3 = t2.then([command_4, timeout, time_spent] () {
                        time_t period_4 = timeout - time_spent;

                        create_child(command_4, period_4);
                    });
                }
            }
        }
        else {
            std::cerr << "config = " << config << " doesn't exist!\n";
        }
    }
}

void create_child(const std::string& command, const time_t& period) {
    std::string line;
    ipstream out;

    child process(command, std_out > out);

    std::thread checkTime(check_time, std::ref(process), std::ref(period));

    while (out && std::getline(out, line) && !line.empty())
        std::cerr << line << std::endl;

    process.wait();

    checkTime.join();
}

void create_child(const std::string& command, const time_t& period, int& res) {
    std::string line;
    ipstream out;

    child process(command, std_out > out);

    std::thread checkTime(check_time, std::ref(process), std::ref(period));

    while (out && std::getline(out, line) && !line.empty())
        std::cerr << line << std::endl;

    process.wait();

    checkTime.join();

    res = process.exit_code();
}

void check_time(child& process, const time_t& period) {
    time_t start =  std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()
    );

    while (true) {
        if ((std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now()) - start > period)
                                                        && process.running()) {
            process.terminate();
            break;
        }
        else if (!process.running()) {
            break;
        }
    }
}
