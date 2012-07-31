/*
* Copyright (c) 2012 Alexander Korsunsky <fat.lobyte9@gmail.com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
* IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdlib>

#include <windows.h>
#include <Psapi.h>

// thanks to http://stackoverflow.com/a/2886589
struct ci_char_traits : public std::char_traits<char> {
    static bool eq(char c1, char c2) { return toupper(c1) == toupper(c2); }
    static bool ne(char c1, char c2) { return toupper(c1) != toupper(c2); }
    static bool lt(char c1, char c2) { return toupper(c1) <  toupper(c2); }
    static int compare(const char* s1, const char* s2, size_t n) {
        while( n-- != 0 ) {
            if( toupper(*s1) < toupper(*s2) ) return -1;
            if( toupper(*s1) > toupper(*s2) ) return 1;
            ++s1; ++s2;
        }
        return 0;
    }
    static const char* find(const char* s, int n, char a) {
        while( n-- > 0 && toupper(*s) != toupper(a) ) {
            ++s;
        }
        return s;
    }
};

typedef std::basic_string<char, ci_char_traits> ci_string;

std::ostream& operator << (std::ostream& os, const ci_string& s)
{ return os<<s.c_str(); }

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



const PriorityClass* get_priorityclass(const ci_string& name)
{    
    const PriorityClass *it = PRIORITYCLASSES;
    for (; it < PRIORITYCLASSES_END; ++it)
        if (name == it->name) return it;
    
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
        std::clog<<"Size for PID Array was insufficient. Retrying with "<<
            maxArraySize * sizeof(DWORD)<<" bytes \n";

        // reallocate with appropriate size
        processIds.reserve(maxArraySize);
            
        if (!EnumProcesses(&processIds[0], maxArraySize*sizeof(DWORD), &bytesReturned))
            return std::vector<DWORD>();
    }
    
    // reset to correct size
    processIds.resize(bytesReturned/sizeof(DWORD));
    
    //std::clog<<"Enumerating processes succeeded. Number of processes: "<<
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
    ci_string target_str;

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
    
    ci_string priostring;
    ci_string whatstring;

    unsigned positional = 0;
    for (int currArg = 1; currArg < argc ; ++currArg)
    {    
        if (get_switch(argv[currArg]) =='h' || get_switch(argv[currArg]) =='?')
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
            // find priority class
            auto p = get_priorityclass(priostring);
            if (p == PRIORITYCLASSES_END)
            {
                std::cerr<<"Priority class "<<priostring<<" uknown.\n";
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
            break;
        }
    }

    return true;
}

ci_string get_process_module_name(DWORD pid)
{
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, // The access to the process object.
        false, // If this value is TRUE, processes created by this process will inherit the handle.
        pid // The identifier of the local process to be opened. 
    );
    if(process == NULL)
        return ci_string();

    const std::size_t BASENAME_MAX = 512;
    char lpBaseName[BASENAME_MAX];
    
    std::size_t len = GetModuleBaseName(process,NULL, lpBaseName, BASENAME_MAX);
    if (!len)
    {
        std::cerr<<"Failed to retrieve executable name for process "
            <<pid<<". ";
        cerr_last_error();
        return ci_string();
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
{
    std::cout<<
R"(WINRENICE [/H | /?] [/V] [/I | /D] <priority> [/E] <target>

Description:
    This program changes the priority class for existing processes.
    Processes can be specified with a numerical Process ID or by the executable
    filename.
    The priority can increased, decreased or set to a specific value.

Parameter list:
    <priority>      If the /I or /D flags are ommited, this the target 
                    process(es) will be set to this priority class. The priority
                    class can be one of:
                        IDLE
                        BELOWNORMAL
                        NORMAL
                        ABOVENORMAL
                        HIGH
                        REALTIME
                        
                    If the either the /I or /D flag is present, <priority> is
                    interpreted as an integer and the priority class of the 
                    target process(es) will be increased or decreased by
                    <priority> steps.
                        
    <target>        Denotes the target process or processes.
                    With the /E flag absent, <target> is the numerical 
                    process ID of the target process.
                    With the /E flag present, <target> is the basename of an
                    executable file. Then, the priority of all processes that
                    were started by this executable file will be changed.
    
    /E              Interpret <target> as the basename of an executable instead
                    of a process ID
    
    /I              Increase priority by <priority> steps
    
    /D              Decrease priority by <priority> steps

    /H or /?        Display program help
    
    /V              Display program version
    
Notes:
    For certain processes (such as system processes), this utility needs
    elevated access privileges or the operation will fail.
    
    The 32-bit version of this utility cannot be used on 64-bit processes.
    Please download the appropriate version for your operating system from
    https://github.com/fat-lobyte/winrenice/downloads .

Examples:
    Set priority of process 2044 to IDLE:
        winrenice IDLE 2044
        
    Set priority of all processes started by "cmd.exe" to HIGH:
        winrenice HIGH /E cmd.exe
    
    Decrease priority of all processes that were started by svchost.exe by 2:
        winrenice /D 2 /E svchost.exe
)";

}

#define PROGRAM_VERSION "0.1"

void display_version()
{
    std::cout<<"winrenice version "<<PROGRAM_VERSION
        <<" by Alexander Korsunsky\n"
        <<"See https://github.com/fat-lobyte/winrenice/ for license and "
          "additional\ninformation.\n";
}

int main(int argc, char *argv[])
{
    ProgramOptions po;

    if (!read_args(po, argc, argv))
    {
        std::cout<<"Pass the /H flag for help on the usage.\n";
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

            if (target_pids.empty())
            {
                std::cerr<<"No processes with executable "<<po.target_str<<" found.\n";
                return EXIT_FAILURE;
            }
            
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
