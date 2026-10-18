package com.brruham.nes;

public class NesCore {
    static { System.loadLibrary("nes"); }
    public static native int load(byte[] rom);
    public static native void reset();
    public static native void runFrame();
    public static native void getFrame(int[] out);
    public static native void setInput(int mask);
}
