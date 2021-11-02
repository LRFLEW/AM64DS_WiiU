#ifndef PROC_HPP
#define PROC_HPP

#include "vpad.hpp"

class WUProc {
public:
    WUProc();
    ~WUProc();
    static WUProc &get_global() noexcept;
    bool update();

    void block_home();
    void release_home();
    void force_release_home();

    void flag_dirty() { dirty = true; }
    bool is_running() { return running; }
    bool is_dirty() { return dirty; }
    bool is_hbl() { return hbc && !dirty; }

private:
    bool hbc;
    bool running = true;
    bool dirty = false;
    bool home = true;
};

class WUHomeLock {
public:
    WUHomeLock(WUProc &proc, Vpad &vpad) : proc(proc), vpad(vpad) { proc.block_home(); }
    ~WUHomeLock() { proc.update(); proc.release_home(); vpad.get(); }

private:
    WUProc &proc;
    Vpad &vpad;
};

#endif // PROC_HPP
