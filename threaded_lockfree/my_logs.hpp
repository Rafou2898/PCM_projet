#define DEBUG_LOGS 1

#if DEBUG_LOGS
#define LOG(msg)                                                                     \
    do                                                                               \
    {                                                                                \
        std::cout << "[T" << std::this_thread::get_id() << "] " << msg << std::endl; \
    } while (0)

#define LOG_STAT(label, value)                                         \
    do                                                                 \
    {                                                                  \
        std::cout << "[STAT] " << label << ": " << value << std::endl; \
    } while (0)
#else
#define LOG(msg) \
    do           \
    {            \
    } while (0)
#define LOG_STAT(label, value) \
    do                         \
    {                          \
    } while (0)
#endif
