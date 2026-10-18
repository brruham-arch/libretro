package com.brruham.nes;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

public class LuaPanel {
    static final StringBuilder LOG = new StringBuilder();
    static final String DEF = "-- Tes dasar SMB: nyawa (tampil = nilai + 1)\nprint(\"script jalan\")\nwhile true do\n  memory.writebyte(0x075A, 98)\n  if emu.framecount() % 120 == 0 then\n    print(\"frame\", emu.framecount(), \"nyawa\", memory.readbyte(0x075A))\n  end\n  emu.frameadvance()\nend\n";

    static String load(Context c) { return c.getSharedPreferences("lua", 0).getString("src", DEF); }
    static void save(Context c, String s) { c.getSharedPreferences("lua", 0).edit().putString("src", s).apply(); }

    static void loadFile(Activity a, Intent d) {
        try {
            InputStream in = a.getContentResolver().openInputStream(d.getData());
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            in.close();
            save(a, new String(bo.toByteArray(), "UTF-8"));
            show(a);
        } catch (Exception e) {
            Toast.makeText(a, "Gagal membaca file", Toast.LENGTH_LONG).show();
        }
    }

    static Button btn(Context cx, String t, View.OnClickListener l) {
        Button b = new Button(cx);
        b.setText(t); b.setTextSize(11); b.setMinWidth(0); b.setMinimumWidth(0);
        b.setPadding(8, 0, 8, 0); b.setOnClickListener(l);
        return b;
    }

    static void addW(LinearLayout r, View v) {
        r.addView(v, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
    }

    static void show(final Activity a) {
        AlertDialog.Builder b = new AlertDialog.Builder(a, android.R.style.Theme_DeviceDefault_Dialog_Alert);
        final Context cx = b.getContext();
        LinearLayout root = new LinearLayout(cx);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 8, 24, 8);
        ScrollView sv = new ScrollView(cx);
        sv.addView(root);

        final EditText ed = new EditText(cx);
        ed.setText(load(cx));
        ed.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        ed.setMinLines(6);
        ed.setMaxLines(10);
        ed.setGravity(Gravity.TOP | Gravity.START);
        ed.setTextSize(12);
        ed.setTypeface(Typeface.MONOSPACE);

        final TextView st = new TextView(cx);
        final TextView log = new TextView(cx);
        log.setTextSize(11);
        log.setTypeface(Typeface.MONOSPACE);
        log.setText(LOG.toString());

        final AlertDialog[] dl = new AlertDialog[1];
        LinearLayout row = new LinearLayout(cx);
        row.setOrientation(LinearLayout.HORIZONTAL);
        addW(row, btn(cx, "JALANKAN", x -> {
            String s = ed.getText().toString();
            save(cx, s);
            NesCore.luaRun(s);
        }));
        addW(row, btn(cx, "STOP", x -> NesCore.luaStop()));
        addW(row, btn(cx, "BUKA FILE", x -> {
            save(cx, ed.getText().toString());
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            dl[0].dismiss();
            a.startActivityForResult(i, 2);
        }));
        addW(row, btn(cx, "HAPUS LOG", x -> { LOG.setLength(0); log.setText(""); }));

        root.addView(ed);
        root.addView(row);
        root.addView(st);
        root.addView(log);

        final Handler h = new Handler(Looper.getMainLooper());
        final Runnable[] tick = new Runnable[1];
        tick[0] = () -> {
            String s = NesCore.luaLog();
            if (s.length() > 0) {
                LOG.append(s);
                if (LOG.length() > 6000) LOG.delete(0, LOG.length() - 4000);
                log.setText(LOG.toString());
            }
            st.setText(NesCore.luaRunning() ? "Status: BERJALAN" : "Status: berhenti");
            h.postDelayed(tick[0], 400);
        };

        b.setView(sv);
        b.setNegativeButton("Tutup", null);
        dl[0] = b.create();
        dl[0].setOnDismissListener(d -> h.removeCallbacks(tick[0]));
        dl[0].show();
        tick[0].run();
    }
}
