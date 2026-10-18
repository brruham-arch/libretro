package com.brruham.nes;

public class NesCore {
    static { System.loadLibrary("nes"); }
    public static native int load(byte[] rom);
    public static native void reset();
    public static native void runFrame();
    public static native void getFrame(int[] out);
    public static native void setInput(int mask);
    public static native void searchReset();
    public static native int searchFilter(int mode, int val);
    public static native int searchCount();
    public static native int[] searchGet(int max);
    public static native void poke(int addr, int val);
    public static native void freeze(int addr, int val, boolean on);
    public static native int[] freezeGet();
    public static native void freezeClear();
    public static native void luaRun(String src);
    public static native void luaStop();
    public static native String luaLog();
    public static native boolean luaRunning();
}
