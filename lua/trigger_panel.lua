-- Panel trigger SMB: uji semua efek lewat tombol di layar.
-- Nanti nama efek di FX dipakai sebagai perintah dari TikTok.
local SPD = 0xF8   -- kecepatan jalan ke kiri (ROM PAL: 0xF6)
local rb, wb = memory.readbyte, memory.writebyte
local FLAG, ID, STATE, DIR = 0x0F, 0x16, 0x1E, 0x46
local XSPD, YSPD = 0x58, 0xA0
local PAGE, X, YHI, Y = 0x6E, 0x87, 0xB6, 0xCF
local BBOX, XMF, YMF1, YMF2, COL = 0x049A, 0x0401, 0x0417, 0x0434, 0x0491

local msg, msgUntil = "", 0
local function say(t) msg = t; msgUntil = emu.framecount() + 120 end
local function inGame() return rb(0x0770) == 1 end

local FX, ORDER = {}, {}
local function fx(name, fn) FX[name] = fn; ORDER[#ORDER + 1] = name end

local function frontPos()
  local px = rb(0x86) + rb(0x6D) * 256
  local dir = (rb(0x33) == 2) and -1 or 1
  local ex = px + dir * 72
  if ex < 0 then ex = 0 end
  local ey = rb(0xCE) - 8
  if rb(0x0754) == 0 then ey = ey + 16 end
  if ey < 0 then ey = 0 end
  if ey > 255 then ey = 255 end
  return ex, ey, dir
end

local function place(s, ex, ey)
  wb(PAGE + s, ex >> 8); wb(X + s, ex & 255); wb(Y + s, ey)
  wb(YHI + s, 1); wb(YSPD + s, 0)
  wb(XMF + s, 0); wb(YMF1 + s, 0); wb(YMF2 + s, 0); wb(COL + s, 0)
end

local function spawnEnemy(e)
  local s
  for i = 0, 4 do if rb(FLAG + i) == 0 then s = i; break end end
  if not s then say("SLOT MUSUH PENUH"); return end
  local ex, ey, dir = frontPos()
  wb(ID + s, e.id); wb(STATE + s, e.state or 0)
  place(s, ex, ey)
  if e.yhi then wb(YHI + s, e.yhi); wb(Y + s, e.y) end
  wb(BBOX + s, e.bbox)
  wb(DIR + s, dir == 1 and 2 or 1)
  wb(XSPD + s, dir == 1 and e.spd or ((256 - e.spd) & 255))
  if e.timer then wb(0x0796 + s, e.timer); wb(0x03A2 + s, 0) end
  wb(FLAG + s, 1)
end

local function spawnItem(t)
  local s = 5
  if rb(FLAG + s) ~= 0 then say("SLOT ITEM PENUH"); return end
  local ex, ey = frontPos()
  wb(0x39, t); wb(ID + s, 0x2E); wb(STATE + s, 0x80)
  place(s, ex, ey)
  wb(BBOX + s, 3); wb(DIR + s, 1); wb(XSPD + s, 0x10)
  wb(FLAG + s, 1)
end

local function setPower(p)
  if p == 0 then wb(0x0756, 0); wb(0x0754, 1)
  else wb(0x0756, p); wb(0x0754, 0) end
end

local FIRST = { 0x25, 0x28, 0x24, 0x22, 0x2A, 0x2E, 0x33, 0x30 }
local function teleport(w)
  if not inGame() then say("MASUK GAME DULU"); return end
  wb(0x075F, w - 1); wb(0x0750, FIRST[w])
  wb(0x0760, 0); wb(0x075C, 0); wb(0x0751, 0); wb(0x0752, 0); wb(0x075B, 0)
  wb(0x075D, 1); wb(0x0757, 1)
  wb(0xFC, 0x80)
  wb(0x0774, 1); wb(0x0772, 0); wb(0x0722, 0)
end

local function enemy(name, e) fx(name, function() spawnEnemy(e) end) end
enemy("GOOMBA",  { id = 0x06, bbox = 0x09, spd = SPD })
enemy("KOOPA H", { id = 0x00, bbox = 0x03, spd = SPD })
enemy("KOOPA M", { id = 0x03, bbox = 0x03, spd = SPD, state = 1 })
enemy("BUZZY",   { id = 0x02, bbox = 0x03, spd = SPD })
enemy("SPINY",   { id = 0x12, bbox = 0x09, spd = SPD })
enemy("HAMMER",  { id = 0x05, bbox = 0x0B, spd = 0, timer = 0x80 })
enemy("PODOBOO", { id = 0x0C, bbox = 0x09, spd = 0, yhi = 2, y = 2 })
fx("ITEM JAMUR", function() spawnItem(0) end)
fx("ITEM API",   function() spawnItem(1) end)
fx("ITEM STAR",  function() spawnItem(2) end)
fx("ITEM 1UP",   function() spawnItem(3) end)
fx("NYAWA +1",   function() wb(0x075A, math.min(rb(0x075A) + 1, 98)); wb(0xFE, 0x40) end)
fx("NYAWA 99",   function() wb(0x075A, 98) end)
fx("KECIL",      function() setPower(0) end)
fx("BESAR",      function() setPower(1) end)
fx("API",        function() setPower(2) end)
fx("KEBAL",      function() wb(0x079F, 0xFF); wb(0xFB, 0x40) end)
fx("WAKTU 400",  function() wb(0x07F8, 4); wb(0x07F9, 0); wb(0x07FA, 0) end)
fx("WAKTU 0",    function() wb(0x07F8, 0); wb(0x07F9, 0); wb(0x07FA, 0) end)
fx("BEKU WAKTU", function() wb(0x0747, 0xFF) end)
fx("MATI", function()
  wb(0x57, 0); wb(0xFC, 1); wb(0x9F, 0xFC)
  wb(0x0E, 0x0B); wb(0x1D, 1); wb(0x0747, 0xFF)
end)
fx("BERSIH",     function() for i = 0, 4 do wb(FLAG + i, 0) end end)
fx("PELURU",     function() wb(0x06CB, 0x17) end)
fx("STOP PELURU", function() wb(0x06CB, 0) end)
fx("WORLD +",    function() teleport((rb(0x075F) + 1) % 8 + 1) end)
fx("WORLD -",    function() teleport((rb(0x075F) - 1) % 8 + 1) end)

local function hudText()
  return string.format("%d-%d L%d T%d C%d", rb(0x075F) + 1, rb(0x075C) + 1, rb(0x075A) + 1,
    rb(0x07F8) * 100 + rb(0x07F9) * 10 + rb(0x07FA), rb(0x075E))
end

local open, hud, page = true, true, 1
local PER = 10
local NP = math.ceil(#ORDER / PER)
print("Panel aktif: " .. #ORDER .. " efek")
while true do
  if hud then gui.text(8, 34, hudText(), 0xFFFFFF) end
  if emu.framecount() < msgUntil then gui.text(8, 44, msg, 0xFFFF00) end
  if gui.button(152, 2, 50, 14, "HUD") then hud = not hud end
  if gui.button(206, 2, 48, 14, "PANEL") then open = not open end
  if open then
    gui.box(132, 18, 124, 114, 0x000000, 120)
    for i = 1, PER do
      local name = ORDER[(page - 1) * PER + i]
      if name then
        local r, c = (i - 1) // 2, (i - 1) % 2
        if gui.button(134 + c * 62, 22 + r * 19, 58, 16, name) then
          local ok, err = pcall(FX[name])
          if ok then say(name) else print(name, err) end
        end
      end
    end
    if gui.button(134, 117, 26, 14, "<") then page = (page - 2) % NP + 1 end
    if gui.button(228, 117, 26, 14, ">") then page = page % NP + 1 end
    gui.text(166, 121, "HAL " .. page .. "/" .. NP, 0xFFFFFF)
  end
  emu.frameadvance()
end
