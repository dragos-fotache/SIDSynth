-- SPDX-License-Identifier: AGPL-3.0-only
-- ReMCP extension for SIDSynth checks in a dedicated scratch song.
local json = require("mcp.json")
local config = assert(io.open(renoise.tool().bundle_path .. "sidsynth-host-root.txt", "r"))
local root = assert(config:read("*l")) .. "/"
config:close()
local title = "SIDSynth Stage 2 host checks"
local sid_path = "Audio/Generators/VST3/ABCDEF019182FAEB5369647353796469"
local render_status = {state = "idle"}

local function reply(value)
  return {content = {{type = "text", text = json.encode(value)}}}
end

local function instrument(song, index)
  local i = math.floor(tonumber(index) or 0) + 1
  assert(i >= 1 and i <= #song.instruments, "Instrument index out of range")
  return song.instruments[i]
end

local function device(song, index)
  local d = instrument(song, index).plugin_properties.plugin_device
  assert(d and d.device_path == sid_path, "Target must be SIDSynth")
  return d
end

local function filename(name, extension)
  assert(type(name) == "string" and name:match("^[%w_-]+$"), "Use a simple scratch filename")
  return root .. name .. extension
end

local function info(song)
  local result = {version = renoise.RENOISE_VERSION, name = song.name, file = song.file_name, playing = song.transport.playing,
    rendering = song.rendering, render = render_status, instruments = {}, plugins = {}, tracks = {}}
  local stat = io.open("/proc/self/stat", "r")
  if stat then result.pid = tonumber(stat:read("*l"):match("^(%d+)")); stat:close() end
  for _, p in ipairs(song.instruments[1].plugin_properties.available_plugin_infos) do
    if p.name:lower():find("sidsynth", 1, true) then result.plugins[#result.plugins + 1] = p end
  end
  for i, value in ipairs(song.instruments) do
    local pp = value.plugin_properties
    local row = {index = i - 1, name = value.name, channel = pp.channel,
      alias = pp.alias_instrument_index, loaded = pp.plugin_loaded}
    if pp.plugin_device then
      row.device = pp.plugin_device.name
      row.path = pp.plugin_device.device_path
      row.editor = pp.plugin_device.external_editor_visible
      row.parameters = {}
      for j, p in ipairs(pp.plugin_device.parameters) do
        row.parameters[#row.parameters + 1] = {index = j, name = p.name, value = p.value,
          text = p.value_string, min = p.value_min, max = p.value_max, default = p.value_default}
      end
    end
    result.instruments[#result.instruments + 1] = row
  end
  for i, track in ipairs(song.tracks) do
    result.tracks[#result.tracks + 1] = {index = i, name = track.name, type = track.type}
  end
  return result
end

local function handler(args)
  local operation = args.operation
  local song = renoise.song()
  if operation == "inspect" then return reply(info(song)) end
  if operation == "reload_tools" then
    local count, errors = require("mcp.router").load_tools_dir(renoise.tool().bundle_path)
    return reply({count = count, errors = errors})
  end
  if operation == "prepare" then
    -- Tag only an empty new song; never repurpose the user's musical work.
    assert(song.name == title or song.name == "" or song.name == "Untitled", "An existing song is open")
    for _, value in ipairs(song.instruments) do
      assert(not value.plugin_properties.plugin_loaded, "A plugin is already loaded")
      for _, sample in ipairs(value.samples) do
        assert(not sample.sample_buffer.has_sample_data, "A sample is already loaded")
      end
    end
    for _, pattern in ipairs(song.patterns) do assert(pattern.is_empty, "Pattern data already exists") end
    song.name = title
    while #song.instruments < 3 do song:insert_instrument_at(#song.instruments + 1) end
    while song.sequencer_track_count < 3 do song:insert_track_at(song.sequencer_track_count + 1) end
    song.transport.bpm = 120
    song.transport.lpb = 4
    song.patterns[1].number_of_lines = 64
    for i = 1, 3 do
      song.instruments[i].name = "SID part " .. i
      song.instruments[i].plugin_properties.channel = i
      song.tracks[i].name = "SID part " .. i
      song.tracks[i].visible_note_columns = 3
    end
    return reply(info(song))
  end
  assert(song.name == title or song.file_name:find(root, 1, true) == 1, "Open the SIDSynth scratch song first")
  if operation == "load_plugin" then
    local value = instrument(song, args.instrument)
    assert(not value.plugin_properties.plugin_device or value.plugin_properties.plugin_device.device_path == sid_path,
      "Refusing to replace a different plugin")
    local path
    for _, p in ipairs(value.plugin_properties.available_plugin_infos) do
      if p.short_name == "SIDSynth" then path = p.path; break end
    end
    assert(path, "SIDSynth is absent from Renoise's available plugins")
    assert(value.plugin_properties:load_plugin(path), "Renoise failed to load SIDSynth")
    value.plugin_properties.auto_suspend = false
  elseif operation == "unload_plugin" then
    local d = device(song, args.instrument)
    d.external_editor_visible = false
    assert(instrument(song, args.instrument).plugin_properties:load_plugin(""), "Could not unload SIDSynth")
  elseif operation == "automation_setup" or operation == "automation_envelope" or operation == "input_inspect" then
    local d = device(song, args.instrument)
    local track = song.tracks[1]
    local path = operation ~= "input_inspect" and "Audio/Effects/Native/*Instr. Automation" or "Audio/Effects/Native/#Sidechain"
    local control, control_index
    for i, candidate in ipairs(track.devices) do
      if candidate.device_path == path then control = candidate; control_index = i; break end
    end
    if not control then control_index = #track.devices + 1; control = track:insert_device_at(path, control_index) end
    if operation == "automation_setup" and args.parameter then
      local parameter_index
      for i, p in ipairs(d.parameters) do if p.name == args.parameter then parameter_index = i - 1; break end end
      assert(parameter_index, "Unknown SIDSynth automation parameter")
      local data = control.active_preset_data
      local count
      data, count = data:gsub("(<ParameterNumber0>)[^<]*(</ParameterNumber0>)", "%1" .. tostring(parameter_index) .. "%2")
      assert(count == 1, "Automation preset mapping field not found")
      control.active_preset_data = data
    end
    if operation == "automation_envelope" then
      assert(control.parameters[1].name == "Filter cutoff", "Map Filter cutoff and allow one idle update first")
      local pattern_track = song.patterns[1].tracks[1]
      local envelope = pattern_track:find_automation(control.parameters[1]) or pattern_track:create_automation(control.parameters[1])
      envelope:clear()
      envelope.playmode = renoise.PatternTrackAutomation.PLAYMODE_POINTS
      envelope:add_point_at(1, 1)
      envelope:add_point_at(9, 0.015)
      envelope:add_point_at(17, 1)
      return reply({points = envelope.points, device_index = control_index, target = control.parameters[1].name})
    end
    local parameters = {}
    for i, p in ipairs(control.parameters) do parameters[#parameters + 1] = {index = i, name = p.name, value = p.value,
      min = p.value_min, max = p.value_max} end
    return reply({preset = control.active_preset_data, parameters = parameters, device_index = control_index,
      output_routings = track.available_output_routings})
  elseif operation == "set_parameter" then
    local d = device(song, args.instrument)
    local found = false
    for _, p in ipairs(d.parameters) do
      if p.name == args.parameter then
        local value = assert(tonumber(args.value), "Missing numeric value")
        assert(value >= p.value_min and value <= p.value_max, "Parameter value outside host range")
        p.value = value
        found = true
        break
      end
    end
    assert(found, "Unknown SIDSynth parameter")
  elseif operation == "channel" then
    local channel = assert(tonumber(args.channel))
    assert(channel >= 1 and channel <= 16 and channel == math.floor(channel), "Invalid MIDI channel")
    instrument(song, args.instrument).plugin_properties.channel = channel
  elseif operation == "editor" then
    device(song, args.instrument).external_editor_visible = args.visible == true
  elseif operation == "midi_control_setup" or operation == "midi_control_inspect" then
    device(song, args.instrument)
    local track = song.tracks[1]
    local path = "Audio/Effects/Native/*Instr. MIDI Control"
    local control
    for _, d in ipairs(track.devices) do if d.device_path == path then control = d; break end end
    if not control then control = track:insert_device_at(path, #track.devices + 1) end
    local data = control.active_preset_data
    if operation == "midi_control_inspect" then return reply({preset = data}) end
    local function field(name, value)
      local count
      data, count = data:gsub("(<" .. name .. ">)[^<]*(</" .. name .. ">)", "%1" .. tostring(value) .. "%2")
      assert(count == 1, "Missing MIDI Control field: " .. name)
    end
    field("ControllerNumber0", 0)
    field("ControllerNumber2", 64)
    field("ControllerName2", "Sustain")
    -- Native device presets omit the instrument link; our scratch song's
    -- device is linked to instrument 00 in its saved Song.xml.
    assert((tonumber(args.instrument) or 0) == 0, "This MIDI Control device targets instrument 00")
    control.active_preset_data = data
    local result = {}
    for i, p in ipairs(control.parameters) do result[#result + 1] = {index = i, name = p.name, value = p.value} end
    return reply(result)
  elseif operation == "note_on" or operation == "note_off" then
    local i = math.floor(tonumber(args.instrument) or 0) + 1
    device(song, args.instrument)
    local note = math.floor(tonumber(args.note) or 69)
    local track = math.floor(tonumber(args.track) or 1)
    if operation == "note_on" then song:trigger_instrument_note_on(i, track, note, 0.8)
    else song:trigger_instrument_note_off(i, track, note) end
  elseif operation == "save" then
    renoise.app():save_song_as(filename(args.name, ".xrns"))
  elseif operation == "save_instrument" or operation == "load_instrument" then
    local target = instrument(song, args.instrument)
    assert(not target.plugin_properties.plugin_device or target.plugin_properties.plugin_device.device_path == sid_path,
      "Refusing to replace a different plugin")
    local previous = song.selected_instrument_index
    song.selected_instrument_index = math.floor(tonumber(args.instrument) or 0) + 1
    local ok, err = pcall(function()
      local path = filename(args.name, ".xrni")
      if operation == "save_instrument" then
        device(song, args.instrument)
        assert(renoise.app():save_instrument(path), "Instrument save failed")
      else
        assert(renoise.app():load_instrument(path), "Instrument load failed")
        device(song, args.instrument)
      end
    end)
    song.selected_instrument_index = previous
    assert(ok, err)
  elseif operation == "load_song" then
    local path = filename(args.name, ".xrns")
    -- Deferring lets the MCP response finish before a document switch.
    local callback
    callback = function()
      renoise.tool():remove_timer(callback)
      renoise.app():load_song(path)
    end
    renoise.tool():add_timer(callback, 100)
    return reply({loading = path})
  elseif operation == "render" then
    if renoise.RENOISE_VERSION:lower():find("demo", 1, true) then
      return reply({state = "unavailable", reason = "Renoise Demo disables offline rendering; use normal playback checks."})
    end
    assert(not song.rendering, "A render is already running")
    local path = filename(args.name, ".wav")
    render_status = {state = "scheduled", file = path}
    local callback
    callback = function()
      renoise.tool():remove_timer(callback)
      local ok, err = song:render({sample_rate = tonumber(args.sample_rate) or 48000,
        bit_depth = 32, priority = "high"}, path, function()
          render_status = {state = "complete", file = path}
        end)
      if not ok then render_status = {state = "failed", error = tostring(err), file = path}
      elseif render_status.state ~= "complete" then render_status.state = "rendering" end
    end
    renoise.tool():add_timer(callback, 100)
    return reply(render_status)
  else error("Unknown host-check operation") end
  return reply(info(song))
end

return {{
  name = "sidsynth_host_check",
  description = "Inspect Renoise or operate SIDSynth in its dedicated scratch song; indices for instruments are 0-based.",
  inputSchema = {type = "object", properties = {
    operation = {type = "string", enum = {"inspect", "prepare", "load_plugin", "set_parameter", "channel",
      "editor", "midi_control_setup", "midi_control_inspect", "note_on", "note_off", "save", "load_song", "render", "reload_tools",
      "unload_plugin", "automation_setup", "automation_envelope", "input_inspect", "save_instrument", "load_instrument"}},
    instrument = {type = "integer"}, track = {type = "integer"}, note = {type = "integer"},
    channel = {type = "integer"}, parameter = {type = "string"}, value = {type = "number"},
    visible = {type = "boolean"}, name = {type = "string"}, sample_rate = {type = "integer"}
  }, required = {"operation"}}, handler = handler
}}
