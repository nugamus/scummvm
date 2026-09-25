/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef X3D_X3D_H
#define X3D_X3D_H

#include "common/array.h"
#include "common/rect.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "engines/engine.h"

#include "x3d/detection.h"
#include "x3d/player.h"
#include "x3d/scene.h"

namespace X3D {

class Collision;
class Interaction;
class Inventory;
class Renderer;
class Scene;
class Sound;
class Talk;
class U01;

class X3DEngine : public Engine {
public:
	X3DEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~X3DEngine() override;

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher;
	}

	// Script primitives for unit code (movement.md, "Scripted camera moves"; u01.md).
	// They run frames until done, like the original's blocking loops: animation, sound
	// and rendering go on, the player's camera input does not.
	void runFor(uint32 ms);   // 0: one frame
	// Moves the camera over ms; nullptr keeps the position, kKeep keeps an angle / the FOV
	void moveTo(uint32 ms, const float *position, float yaw, float pitch, float fov = kKeep);
	void lookAt(uint32 ms, const Math::Vector3d &target);
	void setView(const float *position, float yaw, float pitch); // a cut, no interpolation
	bool enterHeld() const { return _enterHeld; }
	// Suspend: no cursor, camera keys or clicks (u01.md)
	void suspend(bool suspended);
	void gotoScene(const Common::String &name) { _nextScene = name; }
	void addUnitAction(const Common::String &name) { _unitActions.push_back(name); }
	void playVideo(const Common::String &name);

	// Shows a frame until a command is chosen (ui.md); returns the command, "escape", or
	// "enter". The scene, if any, stays frozen underneath.
	Common::String runMenu(const Common::String &name);
	// The Option menu (ui.md): true for New game, false to quit
	bool optionMenu();

	// A debugger command (console.h): where, goto, lookat, click, hotspots, give, hold
	Common::String command(const Common::String &line);

	static constexpr float kKeep = 100.0f;

	// The running scene
	Scene *scene() { return _scene; }
	Player &player() { return _player; }
	Collision *collision() { return _collision; }
	Interaction *interaction() { return _interaction; }
	Inventory *inventory() { return _inventory; }
	Talk *talk() { return _talk; }
	Sound *sound() { return _sound; }
	Renderer *renderer() { return _renderer; }
	const Keys &keys() const { return _keys; }

	// Logic steps per second. The original ran one step per rendered frame; its frame rate
	// is unknown (Q-0022), so this rate is provisional and sets the turn speed.
	static const uint kStepsPerSecond = 60;

protected:
	Common::Error run() override;

private:
	void playScene(const Common::String &sceneName);
	// One pass of the main loop: events, the logic steps due, hover/click, rendering
	void frame(bool input);
	void logicStep(bool input);
	void showBitmap(const Common::Path &path);
	void wait(uint32 ms);

	const ADGameDescription *_gameDescription;
	Renderer *_renderer = nullptr;
	Sound *_sound = nullptr;

	// Scene state, valid inside playScene
	Scene *_scene = nullptr;
	Collision *_collision = nullptr;
	Interaction *_interaction = nullptr;
	Inventory *_inventory = nullptr;
	Talk *_talk = nullptr;
	U01 *_u01 = nullptr;
	Player _player, _previous;
	Keys _keys;
	bool _enterHeld = false, _suspended = false;
	Common::Point _mouse = Common::Point(320, 240);
	bool _hoverNow = false, _clickNow = false;
	int _hotspot = -1;
	uint32 _last = 0, _pending = 0, _logicMs = 0, _lastClick = 0, _frames = 0, _fpsStart = 0;
	Common::Array<int> _devClicks;
	Common::StringArray _devCommands; // "ms:command", from dev_commands
	uint32 _sceneStart = 0;
	Common::String _nextScene;
	Common::StringArray _unitActions;
	Camera _camera; // the last one drawn, for frames over a frozen scene
	Common::String _menuText; // the text edit of the last menu
	bool _escapeNow = false;
};

} // End of namespace X3D

#endif // X3D_X3D_H
