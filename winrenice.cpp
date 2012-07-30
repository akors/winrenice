#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdlib>

#include <windows.h>
#include <Psapi.h>

std::ostream& dout = std::cout; // debug output stream

struct PriorityClass {
    int baseprio;
    const char* name;
    DWORD symbol;
};

static const PriorityClass PRIORITYCLASSES[] = {
    {4, "IDLE", IDLE_PRIORITY_CLASS},
    {6, "BELOWNORMAL", BELOW_NORMAL_PRIORITY_CLASS},
    {8, "NORMAL", NORMAL_PRIORITY_CLASS},
    {10, "ABOVENORMAL", ABOVE_NORMAL_PRIORITY_CLASS},
    {13, "HIGH", HIGH_PRIORITY_CLASS},
    {24, "REALTIME", REALTIME_PRIORITY_CLASS},
};

const std::size_t PRIORITYCLASSES_LENGTH = 
    sizeof(PRIORITYCLASSES)/sizeof(PriorityClass);

const PriorityClass* PRIORITYCLASSES_END =
    PRIORITYCLASSES + PRIORITYCLASSES_LENGTH;



const PriorityClass* get_priorityclass(const char* name)
{    
    const PriorityClass *it = PRIORITYCLASSES;
    for (; it < PRIORITYCLASSES_END; ++it)
        if (!strcmp(name, it->name)) return it;
    
    return it;
}

const PriorityClass* get_priorityclass(DWORD symbol)
{    
    const PriorityClass *it = PRIORITYCLASSES;
    for (; it < PRIORITYCLASSES_END; ++it)
        if (symbol == it->symbol) return it;
    
    return it;
}

const PriorityClass* incdec_priorityclass(
    const PriorityClass* current_class,
    int incdec_by
)
{
    const PriorityClass* new_class = current_class + incdec_by;
    
    if (new_class < PRIORITYCLASSES) return PRIORITYCLASSES;
    else if (new_class >= PRIORITYCLASSES_END) return PRIORITYCLASSES_END - 1;
    else
        return new_class;
}

DWORD cerr_last_error()
{
    DWORD lastError = GetLastError();
    LPVOID lpMsgBuf;
    
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, // The formatting options, and how to interpret the lpSource parameter OMG I FRICKEN HATE MICROSOFT WHY IS THIS SO COMPLICATED
        NULL, // The location of the message definition
        lastError, // The message identifier for the requested message
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // The language identifier for the requested message
        (LPTSTR) &lpMsgBuf, // A pointer to a buffer that receives the null-terminated string that specifies the formatted message
        0, NULL
    ); 
    
    std::cerr<<"Error "<<lastError<<": "<<static_cast<LPCTSTR>(lpMsgBuf);

    LocalFree(lpMsgBuf);
    
    return lastError;
}

std::vector<DWORD> get_all_pids()
{
    const std::size_t PID_ARRAY_SIZE = 1024;

    std::vector<DWORD> processIds(PID_ARRAY_SIZE);
    DWORD bytesReturned;
    DWORD maxArraySize = PID_ARRAY_SIZE;
    
    if (!EnumProcesses(&processIds[0], maxArraySize*sizeof(DWORD), &bytesReturned))
        return std::vector<DWORD>();
    
    // if we don't have enough space, exponentially increase the PID buffer
    while(bytesReturned == maxArraySize*sizeof(DWORD))
    {
        maxArraySize <<= 1;
        dout<<"Size for PID Array was insufficient. Retrying with "<<
            maxArraySize * sizeof(DWORD)<<" bytes \n";

        // reallocate with appropriate size
        processIds.reserve(maxArraySize);
            
        if (!EnumProcesses(&processIds[0], maxArraySize*sizeof(DWORD), &bytesReturned))
            return std::vector<DWORD>();
    }
    
    // reset to correct size
    processIds.resize(bytesReturned/sizeof(DWORD));
    
    //dout<<"Enumerating processes succeeded. Number of processes: "<<
    //    bytesReturned/sizeof(DWORD)<<'\n';
        
    return processIds;
}



template <typename MapType>
typename MapType::const_iterator
find_value(const MapType& m, const typename MapType::mapped_type& value)
{
    auto it = m.begin();
    for (; it != m.end(); ++it)
        if (it->second == value)
            return it;
    
    return it;
}

struct ProgramOptions
{
    bool show_help = false;
    bool show_version = false;

    enum class ReniceWhat {
        pid,
        exe_all
    } what;
        
    DWORD target_pid;
    std::string target_str;

    enum class ReniceHow {
        set,
        increase,
        decrease
    } how;

    union {
        DWORD set;
        int incdec;
    } priority;
};

inline char get_switch(const char* arg)
{
    if (arg[0] != '-' && arg[0] != '/')
        return 0;

    return tolower(arg[1]);
}

bool read_args(ProgramOptions& po, int argc, char *argv[])
{
    po.what = ProgramOptions::ReniceWhat::pid;
    po.how = ProgramOptions::ReniceHow::set;
    
    std::string priostring;
    std::string whatstring;

    unsigned positional = 0;
    for (int currArg = 1; currArg < argc ; ++currArg)
    {    
        if (get_switch(argv[currArg]) == 'h')
        {
            po.show_help = true;
            return true; // stop parsing after help flag
        }
        
        if (get_switch(argv[currArg]) == 'v')
        {
            po.show_version = true;
            return true; // stop parsing after version flag
        }
        
        if (get_switch(argv[currArg]) == 'i')
        {
            po.how = ProgramOptions::ReniceHow::increase;
            continue;
        }

        if (get_switch(argv[currArg]) == 'd')
        {
            po.how = ProgramOptions::ReniceHow::decrease;
            continue;
        }
        
        if (get_switch(argv[currArg]) == 'p')
        {
            po.what = ProgramOptions::ReniceWhat::pid;
            continue;
        }
        
        if (get_switch(argv[currArg]) == 'e')
        {
            po.what = ProgramOptions::ReniceWhat::exe_all;
            continue;
        }
        
        // anything else is not accepted
        if (get_switch(argv[currArg]))
        {
            std::cerr<<"Unknown option \""<<argv[currArg]<<"\"\n";
            return false;
        }
        
        // first argument is the priority
        if (positional == 0)
        {
            priostring = argv[currArg];
            ++positional;
            continue;
        }
        
        // second argument is the target
        if (positional == 1)
        {
            whatstring = argv[currArg];
            ++positional;
            continue;
        }
    }
    
    // if we didn't get two positional arguments, it's not correct
    if (positional < 2)
    {
        std::cerr<<"Not enough input arguments given.\n";
        return false;
    }
    else if( positional > 2)
    {
        std::cerr<<"Too many input arguments given.\n";
        return false;
    }

    // interpret the "how" argument depending on input arguments
    switch (po.how)
    {
        case ProgramOptions::ReniceHow::set:
        {
            // capitalize 
            std::string priostring_capitalized = priostring;
            for (char& c: priostring_capitalized)
                c = toupper(c);

            // find priority class
            auto p = get_priorityclass(priostring_capitalized.c_str());
            if (p == PRIORITYCLASSES_END)
            {
                std::cerr<<"Unknown priority class "<<priostring<<".\n";
                return false;
            }
            
            po.priority.set = p->symbol;
            break;
        }
        case ProgramOptions::ReniceHow::increase:
        case ProgramOptions::ReniceHow::decrease:
        {
            const char *nptr = priostring.c_str();
            char *endptr;
            po.priority.incdec = strtoul(nptr, &endptr, 10);
            if (endptr == nptr)
            {
                std::cerr<<"Priority for increasing or decreasing must be numerical.";
                return false;
            }
            
            if(po.how == ProgramOptions::ReniceHow::decrease)
                po.priority.incdec = -po.priority.incdec;
        }
    }

    // interpret the "what" argument depending on input arguments
    switch(po.what)
    {
        case ProgramOptions::ReniceWhat::pid:
        {
            const char *nptr = whatstring.c_str();
            char *endptr;
            po.target_pid = strtoul(nptr, &endptr, 10);
            if (endptr == nptr)
            {
                std::cerr<<"Process ID must be numerical. "
                    "Add /E or /A switch to look up a process by its executable name.";
                return false;
            }

            break;
        }
        case ProgramOptions::ReniceWhat::exe_all:
        {
            po.target_str = whatstring;
            std::cout<<"after\n";
            break;
        }
    }

    return true;
}

std::string get_process_module_name(DWORD pid)
{
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // The access to the process object.
        false, // If this value is TRUE, processes created by this process will inherit the handle.
        pid // The identifier of the local process to be opened. 
    );
    if(process == NULL)
        return std::string();

    const std::size_t BASENAME_MAX = 512;
    char lpBaseName[BASENAME_MAX];
    
    std::size_t len = GetModuleBaseName(process,NULL, lpBaseName, BASENAME_MAX);
    if (!len)
    {
        std::cerr<<"Failed to retrieve executable name for process "
            <<pid<<". ";
        cerr_last_error();
        return std::string();
    }
        
    // dout<<"Executable name of process "<<pid<<" is. "<<lpBaseName<<'\n';

    return lpBaseName;
}

void renice_one(
    DWORD pid,
    DWORD prio
)
{
    // get a handle to the process
    HANDLE process = OpenProcess(
        PROCESS_SET_INFORMATION, // The access to the process object.
        false, // If this value is TRUE, processes created by this process will inherit the handle.
        pid // The identifier of the local process to be opened. 
    );
    if(process == NULL)
    {
        std::cerr<<"Failed to open process "<<pid<<". ";
        cerr_last_error();
        return;
    }

    if (!SetPriorityClass(process, prio))
    {
        std::cerr<<"Failed to set priority class for process "<<pid<<". ";
        cerr_last_error();
    }
    
    std::cout<<"Setting process "<<pid<<" to priority class "<<
        get_priorityclass(prio)->name<<".\n";

before_exit:
    CloseHandle(process);
}

void incremental_renice_one(
    DWORD pid,
    int incdec_by
)
{
    // get a handle to the process
    HANDLE process = OpenProcess(
        PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, // The access to the process object.
        false, // If this value is TRUE, processes created by this process will inherit the handle.
        pid // The identifier of the local process to be opened. 
    );
    if(process == NULL)
    {
        std::cerr<<"Failed to open process "<<pid<<". ";
        cerr_last_error();
        return;
    }


    DWORD prio = GetPriorityClass(process);
    if (prio == 0)
    {
        std::cerr<<"Failed to retrieve priority class for process "<<pid<<". ";
        cerr_last_error();
        CloseHandle(process);
        return;
    }
    
    auto pp = get_priorityclass(prio);
    if (pp == PRIORITYCLASSES_END)
    {
        std::cerr<<"Unknown priority class returned for process "<<pid<<". ";
        CloseHandle(process);
        return;
    }

    pp = incdec_priorityclass(pp, incdec_by);
    prio = pp->symbol;

    if (!SetPriorityClass(process, prio))
    {
        std::cerr<<"Failed to set priority class for process "<<pid<<". ";
        cerr_last_error();
    }

    std::cout<<"Setting process "<<pid<<" to priority class "<<
        get_priorityclass(prio)->name<<".\n";

    CloseHandle(process);
}

void display_help()
{}

#define PROGRAM_VERSION "0.1"

void display_version()
{
    std::cout<<"winrenice version "<<PROGRAM_VERSION<<" by Alexander Korsunsky\n";
}

int main(int argc, char *argv[])
{
    ProgramOptions po;

    if (!read_args(po, argc, argv))
    {
        display_help();
        return EXIT_FAILURE;
    }
    
    if (po.show_help)
    {
        display_help();
        return EXIT_SUCCESS;
    }
    else if(po.show_version)
    {
        display_version();
        return EXIT_SUCCESS;
    }
    
    
    // get all PID'S
    std::vector<DWORD> all_pids = get_all_pids();
    if (all_pids.empty())
        return cerr_last_error();
    
    // vector for the target PID's
    std::vector<DWORD> target_pids;


    switch(po.what)
    {
        case ProgramOptions::ReniceWhat::pid:
        {
            if (all_pids.end() == 
                std::find(all_pids.begin(), all_pids.end(), po.target_pid)
            )
            {
                std::cerr<<"Process with ID "<<po.target_pid<<" does not exist.\n";
                return EXIT_FAILURE;
            }

            target_pids.push_back(po.target_pid);
            break;
        }
        case ProgramOptions::ReniceWhat::exe_all:
        {
            for(DWORD pid: all_pids)
                if (get_process_module_name(pid) == po.target_str)
                    target_pids.push_back(pid);

            break;
        }
    }


    // set numerical priority depending on command line arguments
    switch(po.how)
    {
        case ProgramOptions::ReniceHow::set:
        {
            for(DWORD pid: target_pids)
                renice_one(pid, po.priority.set);
            
            break;
        }
        case ProgramOptions::ReniceHow::increase:
        case ProgramOptions::ReniceHow::decrease:
        {
            for(DWORD pid: target_pids)
                incremental_renice_one(pid, po.priority.incdec);
            
            break;
        }
    }
    
   

    return EXIT_SUCCESS;
}
