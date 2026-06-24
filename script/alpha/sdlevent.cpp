#include "sdlevent.hpp"

namespace ikemen {

static bool* keyFlag(SdlEventHandler& h, K key)
{
	switch (key) {
	case K::UP:            return &h.upKey;
	case K::DOWN:          return &h.downKey;
	case K::LEFT:          return &h.leftKey;
	case K::RIGHT:         return &h.rightKey;
	case K::ESCAPE:        return &h.esc;
	case K::a:             return &h.aKey;     case K::b: return &h.bKey;
	case K::c:             return &h.cKey;     case K::d: return &h.dKey;
	case K::e:             return &h.eKey;     case K::f: return &h.fKey;
	case K::g:             return &h.gKey;     case K::h: return &h.hKey;
	case K::i:             return &h.iKey;     case K::j: return &h.jKey;
	case K::k:             return &h.kKey;     case K::l: return &h.lKey;
	case K::m:             return &h.mKey;     case K::n: return &h.nKey;
	case K::o:             return &h.oKey;     case K::p: return &h.pKey;
	case K::q:             return &h.qKey;     case K::r: return &h.rKey;
	case K::s:             return &h.sKey;     case K::t: return &h.tKey;
	case K::u:             return &h.uKey;     case K::v: return &h.vKey;
	case K::w:             return &h.wKey;     case K::x: return &h.xKey;
	case K::y:             return &h.yKey;     case K::z: return &h.zKey;
	case K::_0:            return &h.zeroKey;  case K::_1: return &h.oneKey;
	case K::_2:            return &h.twoKey;   case K::_3: return &h.threeKey;
	case K::_4:            return &h.fourKey;  case K::_5: return &h.fiveKey;
	case K::_6:            return &h.sixKey;   case K::_7: return &h.sevenKey;
	case K::_8:            return &h.eightKey; case K::_9: return &h.nineKey;
	case K::KP_0:          return &h.kzeroKey; case K::KP_1: return &h.koneKey;
	case K::KP_2:          return &h.ktwoKey;  case K::KP_3: return &h.kthreeKey;
	case K::KP_4:          return &h.kfourKey; case K::KP_5: return &h.kfiveKey;
	case K::KP_6:          return &h.ksixKey;  case K::KP_7: return &h.ksevenKey;
	case K::KP_8:          return &h.keightKey;case K::KP_9: return &h.knineKey;
	case K::RETURN:        return &h.returnKey;
	case K::BACKSPACE:     return &h.backspaceKey;
	case K::SPACE:         return &h.spaceKey;
	case K::LSHIFT:        return &h.lshiftKey;
	case K::RSHIFT:        return &h.rshiftKey;
	case K::TAB:           return &h.tabKey;
	case K::MINUS:         return &h.minusKey;
	case K::EQUALS:        return &h.equalsKey;
	case K::LEFTBRACKET:   return &h.leftbracketKey;
	case K::RIGHTBRACKET:  return &h.rightbracketKey;
	case K::BACKSLASH:     return &h.backslashKey;
	case K::SEMICOLON:     return &h.semicolonKey;
	case K::COMMA:         return &h.commaKey;
	case K::PERIOD:        return &h.periodKey;
	case K::SLASH:         return &h.slashKey;
	case K::F1:            return &h.f1Key;     case K::F2: return &h.f2Key;
	case K::F3:            return &h.f3Key;     case K::F4: return &h.f4Key;
	case K::F5:            return &h.f5Key;     case K::F9: return &h.f9Key;
	case K::F10:           return &h.f10Key;    case K::F11: return &h.f11Key;
	case K::F12:           return &h.f12Key;
	case K::PRINTSCREEN:   return &h.printscreenKey;
	case K::INSERT:        return &h.insertKey;
	case K::HOME:          return &h.homeKey;
	case K::PAGEUP:        return &h.pageupKey;
	case K::DEL:           return &h.deleteKey;
	case K::END:           return &h.end_Key;
	case K::PAGEDOWN:      return &h.pagedownKey;
	case K::KP_DIVIDE:     return &h.kdivideKey;
	case K::KP_MULTIPLY:   return &h.kmultiplyKey;
	case K::KP_MINUS:      return &h.kminusKey;
	case K::KP_PLUS:       return &h.kplusKey;
	case K::KP_ENTER:      return &h.kenterKey;
	case K::KP_PERIOD:     return &h.kperiodKey;
	default:               return nullptr;
	}
}

void SdlEventHandler::Key::checkDown(K k, uint16_t m)
{
	down |= key == k
	     && shift == ((m & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0)
	     && ctrl  == ((m & (KMOD_LCTRL  | KMOD_RCTRL))  != 0)
	     && alt   == ((m & (KMOD_LALT   | KMOD_RALT))   != 0);
}

SdlEventHandler::SdlEventHandler() { m_event.etype = EventType::NOEVENT; }

void SdlEventHandler::resetKeys()
{
	end = esc = paste = fskip = false;
	upKey = downKey = leftKey = rightKey = false;
	aKey=bKey=cKey=dKey=eKey=fKey=gKey=hKey=iKey=jKey=kKey=lKey=mKey=nKey=oKey=pKey=false;
	qKey=rKey=sKey=tKey=uKey=vKey=wKey=xKey=yKey=zKey=false;
	kzeroKey=koneKey=ktwoKey=kthreeKey=kfourKey=kfiveKey=ksixKey=ksevenKey=keightKey=knineKey=false;
	zeroKey=oneKey=twoKey=threeKey=fourKey=fiveKey=sixKey=sevenKey=eightKey=nineKey=false;
	returnKey=spaceKey=lshiftKey=rshiftKey=backspaceKey=tabKey=false;
	minusKey=equalsKey=leftbracketKey=rightbracketKey=backslashKey=semicolonKey=commaKey=periodKey=slashKey=false;
	f1Key=f2Key=f3Key=f4Key=f5Key=f9Key=f10Key=f11Key=f12Key=false;
	printscreenKey=insertKey=homeKey=pageupKey=deleteKey=end_Key=pagedownKey=false;
	kdivideKey=kmultiplyKey=kminusKey=kplusKey=kenterKey=kperiodKey=false;
	getGamepadKeyA=getGamepadKeyB=getGamepadKeyC=false;
}

bool SdlEventHandler::eventUpdate()
{
	for (auto& k : eventKeys) k.reset();

	while (ikemen::pollEvent(m_event)) {
		switch (m_event.etype) {
		case EventType::QUIT:
			end = true;
			return false;

		case EventType::WINDOWEVENT:
			// UpdateGLViewport handled by SDL
			break;

		case EventType::KEYDOWN: {
			K sym = m_event.key.ks.sym;
			uint16_t mod = m_event.key.ks.mod;

			for (auto& k : eventKeys) k.checkDown(sym, mod);

			if ((mod & (KMOD_LALT | KMOD_RALT)) != 0) {
				if (sym == K::RETURN) {
					full = !full;
					if (ikemen::fullScreen(full))
						ikemen::showCursor(!full || (full && !fullReal));
				} else if (sym == K::F4) {
					end = true;
					return false;
				}
			} else if ((mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0) {
				if (sym == K::v) paste = true;
			} else {
				if (sym == K::ESCAPE) esc = true;
				bool* flag = keyFlag(*this, sym);
				if (flag) *flag = true;
			}
			break;
		}
		default:
			break;
		}

		if (ikemen::joystickButtonState(0, 0)) getGamepadKeyA = true;
		if (ikemen::joystickButtonState(1, 1)) getGamepadKeyB = true;
		if (ikemen::joystickButtonState(2, 2)) getGamepadKeyC = true;
	}
	return !end;
}

bool SdlEventHandler::event(int fps)
{
	uint32_t uWait = 1000u / static_cast<uint32_t>(fps);
	uint32_t now = ikemen::getTicks();

	m_nextTimeFrac += static_cast<float>(1000 % fps) / static_cast<float>(fps);
	if (m_nextTimeFrac >= 1.0f) { m_nextTime++; m_nextTimeFrac -= 1.0f; }

	uint32_t dif = m_nextTime - now;
	m_nextTime += uWait;

	if (dif < uWait + 2) {
		ikemen::delay(dif);
	} else if (now - m_lastDraw <= 250 || dif + 17 >= 17) {
		if (static_cast<int32_t>(-dif) > 150) {
			m_nextTime = now;
			m_nextTime += uWait;
			m_nextTimeFrac += static_cast<float>(1000 % fps) / static_cast<float>(fps);
			if (m_nextTimeFrac >= 1.0f) { m_nextTime++; m_nextTimeFrac -= 1.0f; }
		}
		fskip = true;
	}
	m_lastDraw = now;
	fskip = false;

	resetKeys();
	return eventUpdate();
}

} // namespace ikemen
