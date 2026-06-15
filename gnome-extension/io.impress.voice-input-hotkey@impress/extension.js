import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Clutter from 'gi://Clutter';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const DBUS_IFACE = `
<node>
    <interface name="io.impress.VoiceInputHotkey">
        <signal name="CapsLockPressed"/>
        <signal name="CapsLockReleased"/>
    </interface>
</node>`;

const DBUS_PATH = '/io/impress/VoiceInputHotkey';
const CAPS_LOCK_KEYSYM = 0xffe5;

export default class VoiceInputHotkeyExtension extends Extension {
    constructor(metadata) {
        super(metadata);
        this._dbusNode = null;
        this._captorId = 0;
        this._releaseCaptorId = 0;
        this._retryId = 0;
    }

    enable() {
        try {
            this._dbusNode = Gio.DBusExportedObject.wrapJSObject(DBUS_IFACE, {});
            this._dbusNode.export(Gio.DBus.session, DBUS_PATH);
            this.log('D-Bus 已导出');

            this._tryConnect();
        } catch (e) {
            this.log('enable 失败: ' + e.message);
        }
    }

    _tryConnect() {
        if (this._retryId) {
            GLib.Source.remove(this._retryId);
            this._retryId = 0;
        }

        // 优先使用 uiGroup（始终可用），回退到 global.stage
        let target = null;
        try {
            target = Main.layoutManager?.uiGroup || null;
        } catch (e) {}
        if (!target) {
            try { target = global.stage || null; } catch (e) {}
        }

        if (!target) {
            this.log('uiGroup 和 stage 均不可用，200ms 后重试');
            this._retryId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 200, () => {
                this._retryId = 0;
                this._tryConnect();
                return GLib.SOURCE_REMOVE;
            });
            return;
        }

        try {
            this._captorId = target.connect('captured-event::key-press', (_actor, event) => {
                if (event.type() !== Clutter.EventType.KEY_PRESS) return Clutter.EVENT_PROPAGATE;
                const sym = event.get_key_symbol();
                if (sym === CAPS_LOCK_KEYSYM || sym === Clutter.KEY_Caps_Lock) {
                    this._dbusNode.emit_signal('CapsLockPressed', null);
                    this.log('CapsLock pressed');
                }
                return Clutter.EVENT_PROPAGATE;
            });

            this._releaseCaptorId = target.connect('captured-event::key-release', (_actor, event) => {
                if (event.type() !== Clutter.EventType.KEY_RELEASE) return Clutter.EVENT_PROPAGATE;
                const sym = event.get_key_symbol();
                if (sym === CAPS_LOCK_KEYSYM || sym === Clutter.KEY_Caps_Lock) {
                    this._dbusNode.emit_signal('CapsLockReleased', null);
                    this.log('CapsLock released');
                }
                return Clutter.EVENT_PROPAGATE;
            });

            this.log('全局按键捕获已就绪');
        } catch (e) {
            this.log('连接事件失败: ' + e.message);
            this._retryId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, () => {
                this._retryId = 0;
                this._tryConnect();
                return GLib.SOURCE_REMOVE;
            });
        }
    }

    disable() {
        try {
            if (this._retryId) {
                GLib.Source.remove(this._retryId);
                this._retryId = 0;
            }

            // 从所有可能的目标断开
            let targets = [];
            try { if (Main.layoutManager?.uiGroup) targets.push(Main.layoutManager.uiGroup); } catch (e) {}
            try { if (global.stage) targets.push(global.stage); } catch (e) {}

            for (const t of targets) {
                if (this._captorId) { try { t.disconnect(this._captorId); } catch (e) {} }
                if (this._releaseCaptorId) { try { t.disconnect(this._releaseCaptorId); } catch (e) {} }
            }
            this._captorId = 0;
            this._releaseCaptorId = 0;

            if (this._dbusNode) { this._dbusNode.unexport(); this._dbusNode = null; }
            this.log('Extension disabled');
        } catch (e) {
            this.log('disable 异常: ' + e.message);
        }
    }

    log(msg) {
        console.log(`[ImpressHotkey] ${msg}`);
    }
}
