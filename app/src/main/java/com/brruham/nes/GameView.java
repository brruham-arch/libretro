package com.brruham.nes;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GameView extends SurfaceView implements SurfaceHolder.Callback, Runnable {
    private final int[] pix = new int[256 * 240];
    private final Bitmap bmp = Bitmap.createBitmap(256, 240, Bitmap.Config.ARGB_8888);
    private final Paint game = new Paint();
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txt = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint btxt = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF dst = new RectF(), sel = new RectF(), sta = new RectF(), rom = new RectF(), tmp = new RectF(), chb = new RectF();
    private float W, H, dcx, dcy, dr, ax, ay, bx, by, br;
    private volatile boolean running, surfaceOk, loaded, paused;
    private Thread thread;
    private volatile int pad = 0;

    public GameView(Context c) {
        super(c);
        getHolder().addCallback(this);
        game.setFilterBitmap(false);
        txt.setColor(Color.WHITE);
        txt.setTextAlign(Paint.Align.CENTER);
        btxt.setColor(0xFF202020);
        btxt.setTextAlign(Paint.Align.CENTER);
        btxt.setFakeBoldText(true);
    }

    public void setLoaded(boolean v) { loaded = v; }
    public void pauseEmu() { paused = true; halt(); }
    public void resumeEmu() { paused = false; begin(); }

    public synchronized void begin() {
        if (running || !loaded || !surfaceOk || paused) return;
        running = true;
        thread = new Thread(this);
        thread.start();
    }

    public synchronized void halt() {
        running = false;
        if (thread != null) {
            try { thread.join(); } catch (InterruptedException e) { }
            thread = null;
        }
    }

    @Override public void surfaceCreated(SurfaceHolder h) { surfaceOk = true; begin(); }

    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int hh) {
        W = w; H = hh;
        float s = Math.min(W / 256f, H / 240f);
        float gw = 256 * s, gh = 240 * s;
        dst.set((W - gw) / 2, (H - gh) / 2, (W + gw) / 2, (H + gh) / 2);
        dr = H * 0.15f; dcx = dr * 1.9f; dcy = H - dr * 1.9f;
        br = H * 0.09f; ax = W - br * 1.8f; ay = H - br * 3.3f; bx = W - br * 4.8f; by = H - br * 1.9f;
        float bw = H * 0.2f, bh = H * 0.07f, g = H * 0.02f, bot = H - H * 0.04f;
        sel.set(W / 2 - bw - g, bot - bh, W / 2 - g, bot);
        sta.set(W / 2 + g, bot - bh, W / 2 + g + bw, bot);
        rom.set(H * 0.03f, H * 0.03f, H * 0.19f, H * 0.03f + bh); chb.set(H * 0.21f, H * 0.03f, H * 0.37f, H * 0.03f + bh);
        if (!running) redraw();
    }

    @Override public void surfaceDestroyed(SurfaceHolder h) { halt(); surfaceOk = false; }

    private void redraw() {
        Canvas c = null;
        try {
            c = (android.os.Build.VERSION.SDK_INT >= 26) ? getHolder().lockHardwareCanvas() : getHolder().lockCanvas();
            if (c != null) render(c);
        } catch (Exception e) {
        } finally {
            if (c != null) getHolder().unlockCanvasAndPost(c);
        }
    }

    @Override public void run() {
        final long FRAME = 16639267L;
        long next = System.nanoTime();
        while (running) {
            NesCore.runFrame();
            NesCore.getFrame(pix);
            bmp.setPixels(pix, 0, 256, 0, 0, 256, 240);
            redraw();
            next += FRAME;
            long now = System.nanoTime(), sl = next - now;
            if (sl > 0) {
                try { Thread.sleep(sl / 1000000L, (int) (sl % 1000000L)); } catch (InterruptedException e) { }
            } else if (sl < -100000000L) next = now;
        }
    }

    private void render(Canvas c) {
        c.drawColor(Color.BLACK);
        if (loaded) c.drawBitmap(bmp, null, dst, game);
        else {
            txt.setTextSize(H * 0.06f);
            c.drawText("Tap ROM untuk membuka file .nes", W / 2, H / 2, txt);
        }
        int p = pad;
        float t = dr * 0.38f;
        fill.setColor(0x33FFFFFF);
        c.drawCircle(dcx, dcy, dr * 1.5f, fill);
        tmp.set(dcx - t, dcy - dr * 1.35f, dcx + t, dcy - dr * 0.1f); rect(c, tmp, (p & 0x10) != 0, "");
        tmp.set(dcx - t, dcy + dr * 0.1f, dcx + t, dcy + dr * 1.35f); rect(c, tmp, (p & 0x20) != 0, "");
        tmp.set(dcx - dr * 1.35f, dcy - t, dcx - dr * 0.1f, dcy + t); rect(c, tmp, (p & 0x40) != 0, "");
        tmp.set(dcx + dr * 0.1f, dcy - t, dcx + dr * 1.35f, dcy + t); rect(c, tmp, (p & 0x80) != 0, "");
        circle(c, ax, ay, br, (p & 1) != 0, "A");
        circle(c, bx, by, br, (p & 2) != 0, "B");
        rect(c, sel, (p & 4) != 0, "SELECT");
        rect(c, sta, (p & 8) != 0, "START");
        rect(c, chb, false, "CHEAT");
    }

    private void circle(Canvas c, float x, float y, float r, boolean on, String s) {
        fill.setColor(on ? 0xAAFFFFFF : 0x55FFFFFF);
        c.drawCircle(x, y, r, fill);
        btxt.setTextSize(r * 0.9f);
        c.drawText(s, x, y + r * 0.32f, btxt);
    }

    private void rect(Canvas c, RectF r, boolean on, String s) {
        fill.setColor(on ? 0xAAFFFFFF : 0x55FFFFFF);
        c.drawRoundRect(r, 16, 16, fill);
        if (s.length() > 0) {
            btxt.setTextSize(r.height() * 0.45f);
            c.drawText(s, r.centerX(), r.centerY() + r.height() * 0.16f, btxt);
        }
    }

    private boolean hit(float x, float y, float cx, float cy, float r) {
        float dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy < r * r;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int act = e.getActionMasked();
        if (act == MotionEvent.ACTION_DOWN || act == MotionEvent.ACTION_POINTER_DOWN) {
            int ai = e.getActionIndex();
            if (rom.contains(e.getX(ai), e.getY(ai))) { ((MainActivity) getContext()).pick(); return true; }
            if (chb.contains(e.getX(ai), e.getY(ai))) { ((MainActivity) getContext()).cheat(); return true; }
        }
        int skip = (act == MotionEvent.ACTION_UP || act == MotionEvent.ACTION_POINTER_UP) ? e.getActionIndex() : -1;
        int m = 0;
        if (act != MotionEvent.ACTION_CANCEL) {
            for (int i = 0; i < e.getPointerCount(); i++) {
                if (i == skip) continue;
                float x = e.getX(i), y = e.getY(i);
                float dx = x - dcx, dy = y - dcy;
                if (dx * dx + dy * dy < dr * dr * 3f) {
                    if (dx > dr * 0.35f) m |= 0x80; else if (dx < -dr * 0.35f) m |= 0x40;
                    if (dy > dr * 0.35f) m |= 0x20; else if (dy < -dr * 0.35f) m |= 0x10;
                }
                if (hit(x, y, ax, ay, br * 1.3f)) m |= 1;
                if (hit(x, y, bx, by, br * 1.3f)) m |= 2;
                if (sel.contains(x, y)) m |= 4;
                if (sta.contains(x, y)) m |= 8;
            }
        }
        pad = m;
        NesCore.setInput(m);
        if (!running) redraw();
        return true;
    }
}
