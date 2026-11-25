"""French operator interface. All serial operations run outside Tk's UI thread."""
from datetime import datetime
from pathlib import Path
import queue
import threading
import tkinter as tk
from tkinter import filedialog, ttk

import wrist_bench as bench
from wrist_motion import Cycle, MotionRunner, run_cycle, run_move

ROOT = Path(__file__).resolve().parent

FAULT_LABELS = {
    "uart_error": "Erreur de liaison série : vérifier l'USB et le port",
    "communication_timeout": "Liaison PC interrompue",
    "position_limit": "Limite de position dépassée",
    "velocity_limit": "Vitesse maximale dépassée",
    "stall": "Moteur bloqué ou mouvement insuffisant",
    "motion_timeout": "Position non atteinte dans le délai prévu",
    "hold_error": "Position de maintien perdue",
    "estop": "Arrêt d'urgence mémorisé",
    "rx_overrun": "Réception série saturée",
    "loop_timeout": "Boucle de contrôle trop lente",
    "counter_delta": "Variation anormale du compteur encodeur",
    "pid_invalid": "Calcul PID invalide",
    "encoder_invalid": "Retour encodeur invalide",
}


class WristApp:
    def __init__(self, root, args):
        self.root, self.args = root, args
        self.session = self.recorder = self.runner = None
        self.connect_thread = None
        self.connect_cancel = threading.Event()
        self.connecting = self.closing = False
        self.events = queue.Queue()
        self.plot_window = None
        root.title("Mechanical Wrist — Banc de test")
        root.geometry("960x810")
        root.minsize(870, 760)
        root.protocol("WM_DELETE_WINDOW", self.close)
        style = ttk.Style(root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Segoe UI", 19, "bold"))
        style.configure("Status.TLabel", font=("Segoe UI", 11, "bold"))
        style.configure("Hint.TLabel", foreground="#505c6d")
        style.configure("TLabelframe.Label", font=("Segoe UI", 10, "bold"))
        self.port = tk.StringVar(value=args.port or "")
        self.count = tk.StringVar(value="1")
        self.flexion = tk.StringVar(value=self._default(bench.config.CYCLE_FLEXION_DEG))
        self.extension = tk.StringVar(value=self._default(bench.config.CYCLE_EXTENSION_DEG))
        self.angle = tk.StringVar(value="")
        self.rotation = tk.StringVar(value="180")
        self.state = tk.StringVar(value="Non connecté")
        self.positions = tk.StringVar(value="Flexion : —     Rotation : —     |     M1 : —     M2 : —")
        self.guidance = tk.StringVar(value="Choisir le port ST-LINK, puis Connecter.")
        self.progress_text = tk.StringVar(value="Prêt à préparer le banc.")
        self.recording = tk.StringVar(value="Les mesures seront enregistrées automatiquement en CSV.")
        self.notice = tk.StringVar(value="")
        self.last_fault = "none"
        self._build()
        self.refresh_ports()
        self._tick()

    @staticmethod
    def _default(value):
        return "" if value is None else str(value)

    def _build(self):
        frame = ttk.Frame(self.root, padding=18)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(8, weight=1)
        ttk.Label(frame, text="Mechanical Wrist", style="Title.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(frame, text="Flexion / extension : sens opposés   •   Rotation : même sens", style="Hint.TLabel").grid(
            row=1, column=0, sticky="w", pady=(2, 12))

        connection = ttk.Frame(frame)
        connection.grid(row=2, column=0, sticky="ew", pady=(0, 10))
        ttk.Label(connection, text="Port ST-LINK").pack(side="left", padx=(0, 8))
        self.port_box = ttk.Combobox(connection, textvariable=self.port, width=36, state="readonly")
        self.port_box.pack(side="left", padx=(0, 6))
        self.refresh_button = ttk.Button(connection, text="Actualiser", command=self.refresh_ports)
        self.refresh_button.pack(side="left", padx=3)
        self.connect_button = ttk.Button(connection, text="Connecter", command=self.connect)
        self.connect_button.pack(side="left", padx=3)
        self.disconnect_button = ttk.Button(connection, text="Déconnecter", command=self.disconnect)
        self.disconnect_button.pack(side="left", padx=3)

        status = ttk.Frame(frame)
        status.grid(row=3, column=0, sticky="ew", pady=(0, 10))
        ttk.Label(status, textvariable=self.state, style="Status.TLabel").pack(anchor="w")
        ttk.Label(status, textvariable=self.positions).pack(anchor="w", pady=3)
        ttk.Label(status, textvariable=self.guidance, style="Hint.TLabel", wraplength=880).pack(anchor="w")

        prep = ttk.LabelFrame(frame, text="1  Préparer le banc", padding=10)
        prep.grid(row=4, column=0, sticky="ew", pady=(0, 10))
        ttk.Label(prep, text="Puissance moteur coupée : charger le profil, placer manuellement au neutre, puis définir le zéro.",
                  wraplength=850).pack(anchor="w", pady=(0, 6))
        buttons = ttk.Frame(prep)
        buttons.pack(fill="x")
        self.calibrate_button = ttk.Button(buttons, text="Charger le profil encodeurs", command=self.calibrate)
        self.zero_button = ttk.Button(buttons, text="Définir le neutre + réglages", command=self.zero_setup)
        self.arm_button = ttk.Button(buttons, text="Armer les moteurs", command=lambda: self.command("arm", "Moteurs armés. Choisir un mouvement."))
        self.clear_button = ttk.Button(buttons, text="Effacer le défaut", command=lambda: self.command("clear_fault", "Défaut effacé. Refaire le neutre + réglages."))
        for button in (self.calibrate_button, self.zero_button, self.arm_button, self.clear_button):
            button.pack(side="left", padx=(0, 8))

        cycle = ttk.LabelFrame(frame, text="2  Cycle automatique", padding=10)
        cycle.grid(row=5, column=0, sticky="ew", pady=(0, 10))
        ttk.Label(cycle, text="Flexion → Extension → Neutre → Rotation 180° → Neutre", font=("Segoe UI", 11, "bold")).pack(anchor="w")
        ttk.Label(cycle, text="Saisir deux amplitudes positives : 20 et 15 donnent +20° puis −15°. Le sens est appliqué automatiquement.",
                  style="Hint.TLabel", wraplength=850).pack(anchor="w", pady=(3, 8))
        fields = ttk.Frame(cycle)
        fields.pack(fill="x")
        for label, variable, width in (("Cycles", self.count, 7), ("Flexion (° positifs)", self.flexion, 8),
                                       ("Extension (° positifs)", self.extension, 8)):
            ttk.Label(fields, text=label).pack(side="left", padx=(0, 6))
            ttk.Entry(fields, textvariable=variable, width=width).pack(side="left", padx=(0, 16))
        self.cycle_button = ttk.Button(fields, text="Démarrer le cycle", command=self.start_cycle)
        self.cycle_button.pack(side="left")
        self.progress = ttk.Progressbar(cycle, mode="determinate", maximum=1)
        self.progress.pack(fill="x", pady=(10, 4))
        ttk.Label(cycle, textvariable=self.progress_text).pack(anchor="w")

        manual = ttk.LabelFrame(frame, text="3  Mouvements simples", padding=10)
        manual.grid(row=6, column=0, sticky="ew", pady=(0, 10))
        row = ttk.Frame(manual)
        row.pack(fill="x")
        ttk.Label(row, text="Amplitude (° positifs)").pack(side="left", padx=(0, 6))
        ttk.Entry(row, textvariable=self.angle, width=7).pack(side="left", padx=(0, 8))
        self.flex_button = ttk.Button(row, text="Flexion +", command=lambda: self.move("flexion", self.angle.get()))
        self.ext_button = ttk.Button(row, text="Extension −", command=lambda: self.move("extension", self.angle.get()))
        self.flex_button.pack(side="left", padx=(0, 6))
        self.ext_button.pack(side="left", padx=(0, 18))
        ttk.Label(row, text="Rotation (° signés)").pack(side="left", padx=(0, 6))
        ttk.Entry(row, textvariable=self.rotation, width=7).pack(side="left", padx=(0, 8))
        self.rot_button = ttk.Button(row, text="Tourner", command=lambda: self.move("rotation", self.rotation.get()))
        self.rot_button.pack(side="left", padx=(0, 6))
        self.neutral_button = ttk.Button(row, text="Retour au neutre", command=lambda: self.move("neutre"))
        self.neutral_button.pack(side="left")
        ttk.Label(manual, text="Amplitude positive (ex. 10) : le bouton choisit le sens. Rotation : ex. 90 ou −90. Cibles absolues, autre axe à 0 ; degrés de sortie des réducteurs.",
                  style="Hint.TLabel", wraplength=850).pack(anchor="w", pady=(6, 0))

        controls = ttk.Frame(frame)
        controls.grid(row=7, column=0, sticky="ew", pady=(0, 8))
        self.stop_button = ttk.Button(controls, text="Arrêter / libérer les moteurs", command=self.stop)
        self.stop_button.pack(side="left", ipady=6)
        self.estop_button = tk.Button(controls, text="ARRÊT D'URGENCE", bg="#b42324", fg="white",
                                     activebackground="#8d191a", activeforeground="white", relief="flat",
                                     font=("Segoe UI", 10, "bold"), padx=16, pady=7,
                                     command=lambda: self.stop(emergency=True))
        self.estop_button.pack(side="left", padx=12)
        self.graph_button = ttk.Button(controls, text="Courbes", command=self.show_plots)
        self.graph_button.pack(side="right", padx=(6, 0))
        self.export_button = ttk.Button(controls, text="Exporter PNG", command=self.export_plot)
        self.export_button.pack(side="right")

        log_frame = ttk.Frame(frame)
        log_frame.grid(row=8, column=0, sticky="nsew")
        ttk.Label(log_frame, textvariable=self.notice, foreground="#b42324", wraplength=880).pack(anchor="w")
        self.log = tk.Text(log_frame, height=4, wrap="word", state="disabled", font=("Consolas", 9))
        self.log.pack(fill="both", expand=True, pady=(4, 2))
        ttk.Label(frame, textvariable=self.recording, style="Hint.TLabel", wraplength=880).grid(row=9, column=0, sticky="w")

    def log_message(self, message, error=False):
        self.notice.set(message if error else "")
        self.log.configure(state="normal")
        self.log.insert("end", f"{datetime.now():%H:%M:%S}  {message}\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def refresh_ports(self):
        try:
            from serial.tools import list_ports
            ports = list(list_ports.comports())
            self.port_names = {f"{p.device} — {p.description}": p.device for p in ports}
            self.port_box["values"] = list(self.port_names)
            chosen = next((name for name, device in self.port_names.items() if device == self.port.get()), None)
            if chosen:
                self.port.set(chosen)
            elif not self.port.get() and len(ports) == 1:
                self.port.set(next(iter(self.port_names)))
            if not ports:
                self.log_message("Aucun port série détecté. Brancher la Nucleo par son USB ST-LINK, puis Actualiser.")
        except Exception as exc:
            self.log_message(str(exc), error=True)

    def connect(self):
        if self.session or self.connecting:
            return
        port = self.port_names.get(self.port.get(), self.port.get())
        if not port:
            self.log_message("Choisir le port série ST-LINK.", error=True)
            return
        self.connecting = True
        self.connect_cancel.clear()
        self.state.set("Connexion en cours…")
        self.connect_thread = threading.Thread(target=self._connect, args=(port,), daemon=True)
        self.connect_thread.start()

    def _connect(self, port):
        session = recorder = serial_port = None
        try:
            import serial
            path = Path(self.args.csv) if self.args.csv else ROOT / "recordings" / f"wrist_{datetime.now():%Y%m%d_%H%M%S_%f}.csv"
            path.parent.mkdir(parents=True, exist_ok=True)
            recorder = bench.CSVRecorder(path)
            serial_port = serial.Serial(port, bench.config.BAUDRATE, timeout=.02, write_timeout=.2)
            session = bench.SerialSession(serial_port, recorder)
            session.send("stop")
            session.start()
            session.snapshot(cancel=self.connect_cancel)
            session.command(f"stream on {bench.config.TELEMETRY_HZ}", cancel=self.connect_cancel)
            if self.connect_cancel.is_set():
                raise RuntimeError("Connexion annulée.")
            self.events.put(dict(kind="connected", session=session, recorder=recorder, path=path))
        except Exception as exc:
            if session:
                session.close()
            elif serial_port:
                serial_port.close()
            if recorder:
                recorder.close()
            self.events.put(dict(kind="connect_error", message=str(exc)))

    def submit(self, action):
        if not self.session or self.session.stop_event.is_set():
            self.log_message("Connecter la carte avant de continuer.", error=True)
            return
        session = self.session
        def work(cancel):
            message = action(session, cancel)
            session.snapshot(cancel=cancel)
            return message
        try:
            self.runner.start(work)
            self.notice.set("")
        except ValueError as exc:
            self.log_message(str(exc), error=True)

    def command(self, text, message):
        def action(session, cancel):
            session.command(text, cancel=cancel)
            return message
        self.submit(action)

    def calibrate(self):
        def action(session, cancel):
            bench.apply_configuration(session, calibration=True, cancel=cancel)
            return "Profil encodeurs chargé. Placer manuellement au neutre, puis Définir le neutre + réglages."
        self.submit(action)

    def zero_setup(self):
        def action(session, cancel):
            bench.configuration_commands()  # Validate before invalidating the zero/limits.
            session.command("zero", cancel=cancel)
            bench.apply_configuration(session, cancel=cancel)
            return "Neutre défini et réglages appliqués. Préparer la puissance moteur avant d'armer."
        self.submit(action)

    def start_cycle(self):
        try:
            cycle = Cycle(int(self.count.get()), float(self.flexion.get().replace(",", ".")),
                          float(self.extension.get().replace(",", ".")))
        except ValueError as exc:
            self.log_message(f"Vérifier le nombre de cycles et les deux amplitudes. {exc}", error=True)
            return
        self.submit(lambda session, cancel: run_cycle(session, cycle, cancel, self.events.put,
                                                    bench.config.TELEMETRY_HZ, bench.motion_profile()))

    def move(self, kind, value=None):
        try:
            step = bench.simple_step([kind] if value is None else [kind, value])
        except ValueError as exc:
            self.log_message(str(exc), error=True)
            return
        self.submit(lambda session, cancel: run_move(session, step, cancel, bench.config.TELEMETRY_HZ,
                                                   bench.motion_profile()))

    def stop(self, emergency=False):
        if self.runner and not self.session.stop_event.is_set():
            try:
                self.runner.stop(emergency)
                self.log_message("Arrêt d'urgence : défaut mémorisé. Corriger la cause avant de l'effacer." if emergency else
                                 "Arrêt demandé. Les moteurs sont désarmés ; aucun retour automatique au neutre.")
                self.progress_text.set("Opération interrompue.")
            except Exception as exc:
                self.log_message(str(exc), error=True)

    def show_plots(self):
        if self.plot_window and self.plot_window.winfo_exists():
            self.plot_window.lift()
            return
        from matplotlib.figure import Figure
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        window = self.plot_window = tk.Toplevel(self.root)
        window.title("Mechanical Wrist — Mesures des deux moteurs")
        fig = Figure(figsize=(11, 8), layout="constrained")
        canvas = FigureCanvasTkAgg(fig, master=window)
        canvas.get_tk_widget().pack(fill="both", expand=True)
        axes = fig.subplots(4, 2, sharex=True)
        def redraw():
            if window.winfo_exists():
                bench.draw_samples(axes, self.session.sample_copy() if self.session else [])
                canvas.draw_idle()
                window.after(700, redraw)
        redraw()

    def export_plot(self):
        path = filedialog.asksaveasfilename(parent=self.root, title="Exporter les courbes", defaultextension=".png",
                                          filetypes=[("Image PNG", "*.png")])
        if path:
            def action(session, cancel):
                bench.save_plot(session.sample_copy(), path)
                return f"Courbes exportées : {path}"
            self.submit(action)

    def _tick(self):
        if self.closing:
            return
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                break
            kind = event["kind"]
            if kind == "connected":
                self.session, self.recorder = event["session"], event["recorder"]
                self.runner = MotionRunner(self.session, self.events.put)
                self.connecting = False
                self.last_fault = "none"
                self.recording.set(f"CSV : {event['path']}")
                self.log_message("Carte connectée, moteurs désarmés.")
                self.log_message(f"Client chargé : rampe {bench.config.MOTION_SPEED_DEG_S:g}°/s, "
                                 f"accélération {bench.config.MOTION_ACCEL_DEG_S2:g}°/s².")
                if self.session.decoder.discarded_prefix:
                    self.log_message("Synchronisation série : premier fragment incomplet ignoré avant la lecture du statut.")
            elif kind == "connect_error":
                self.connecting = False
                self.state.set("Connexion impossible")
                self.log_message(event["message"], error=True)
            elif kind == "progress":
                self.progress.configure(maximum=event["total"], value=event["completed"])
                self.progress_text.set(f"Cycle {event['cycle']}/{event['total']} — {event['stage']} · {event['completed']} terminé(s)")
            else:
                self.log_message(event["message"], error=kind == "error")
                if kind in ("cancelled", "error"):
                    self.progress_text.set("Opération interrompue ; consulter le message ci-dessous.")
        self._update_status()
        self.tick_id = self.root.after(100, self._tick)

    @staticmethod
    def _enable(widget, enabled):
        widget.configure(state="normal" if enabled else "disabled")

    def _update_status(self):
        live = self.session is not None and not self.session.stop_event.is_set()
        packet = self.session.current()[0] if live else None
        busy = self.runner is not None and self.runner.busy
        armed = bool(packet and packet["armed"])
        healthy = bool(packet and packet["fault"] == "none")
        idle = live and not busy
        detailed = self.session.latest_status if live else None
        calibrated = bool(packet and all(m["calibrated"] for m in packet["motors"]))
        ready = bool(detailed and packet and calibrated and all(
            m["zeroed"] and m["feedback_valid"] for m in packet["motors"]) and all(
            all(m.get("configuration", {}).get(k) for k in ("limits_set", "pid_set", "safety_set"))
            for m in detailed["motors"]))
        self._enable(self.connect_button, self.session is None and not self.connecting)
        self._enable(self.disconnect_button, self.session is not None)
        self._enable(self.refresh_button, self.session is None and not self.connecting)
        self.port_box.configure(state="readonly" if self.session is None and not self.connecting else "disabled")
        self._enable(self.calibrate_button, idle and healthy and not armed)
        self._enable(self.zero_button, idle and healthy and not armed and calibrated)
        self._enable(self.arm_button, idle and healthy and not armed and ready)
        self._enable(self.clear_button, idle and not healthy and not armed)
        for button in (self.cycle_button, self.flex_button, self.ext_button, self.rot_button, self.neutral_button):
            self._enable(button, idle and healthy and armed)
        for button in (self.stop_button, self.estop_button):
            self._enable(button, live)
        self._enable(self.graph_button, live)
        self._enable(self.export_button, idle)
        if self.session and not live:
            self.state.set("Connexion perdue — moteurs arrêtés par le watchdog")
            self.guidance.set("Déconnecter puis reconnecter. La reprise et l'armement restent manuels.")
            return
        if not packet:
            return
        states = {"disarmed": "Désarmé", "armed": "Armé — prêt à lancer un mouvement", "moving": "Mouvement en cours",
                  "holding": "Position atteinte — maintien actif", "fault": "Défaut"}
        if healthy:
            self.state.set(states[packet["state"]])
        else:
            source = f"M{packet['fault_motor']}" if packet["fault_motor"] else "global"
            label = FAULT_LABELS.get(packet["fault"], packet["fault"])
            message = f"Défaut {source} : {label} ({packet['fault']})"
            self.state.set(message)
            if packet["fault"] != self.last_fault:
                self.log_message(message + ". " + bench.fault_diagnostic(packet, self.session.latest_status), error=True)
        self.last_fault = packet["fault"]
        a, b = (m["position_deg"] for m in packet["motors"])
        if a is not None and b is not None:
            self.positions.set(f"Flexion : {(a-b)/2:.2f}°     Rotation : {(a+b)/2:.2f}°     |     M1 : {a:.2f}°     M2 : {b:.2f}°")
        else:
            self.positions.set("Angles indisponibles — charger une calibration encodeurs valide.")
        if not healthy:
            self.guidance.set("Corriger la cause du défaut, puis Effacer le défaut et refaire le neutre + réglages.")
        elif not calibrated:
            self.guidance.set("1. Vérifier le profil encodeurs dans bench_config.py (PPR par défaut non confirmé), puis Charger le profil encodeurs.")
        elif not ready:
            self.guidance.set("2. Puissance moteur coupée : placer manuellement au neutre, puis Définir le neutre + réglages.")
        elif not armed:
            self.guidance.set("3. Montage et alimentation prêts : Armer les moteurs, puis choisir un cycle ou un mouvement simple.")
        else:
            self.guidance.set("Après un mouvement, le maintien reste actif. Arrêter libère les deux moteurs.")

    def disconnect(self):
        if not self.session:
            return
        session = self.session
        self.stop()
        self.runner.join()
        session.close()
        if self.recorder:
            self.recorder.close()
        if self.args.save_plot and session.sample_copy():
            try:
                bench.save_plot(session.sample_copy(), self.args.save_plot)
            except Exception as exc:
                self.log_message(str(exc), error=True)
        self.session = self.recorder = self.runner = None
        self.state.set("Non connecté")
        self.positions.set("Flexion : —     Rotation : —     |     M1 : —     M2 : —")
        self.guidance.set("Choisir le port ST-LINK, puis Connecter.")
        self.log_message("Carte déconnectée. CSV conservé sur disque.")

    def close(self):
        self.closing = True
        if hasattr(self, "tick_id"):
            self.root.after_cancel(self.tick_id)
        self.connect_cancel.set()
        if self.connect_thread:
            self.connect_thread.join(timeout=3)
        # A connection can complete just before the close request is processed.
        while not self.events.empty():
            event = self.events.get_nowait()
            if event["kind"] == "connected":
                event["session"].close()
                event["recorder"].close()
        self.disconnect()
        self.root.destroy()


def launch(args):
    try:
        root = tk.Tk()
    except tk.TclError as exc:
        print(f"Interface indisponible : vérifier l'installation Python / Tcl-Tk.\n{exc}\n"
              "La console reste accessible avec --console --port COMx.", file=bench.sys.stderr)
        return 1
    app = WristApp(root, args)
    try:
        root.mainloop()
    finally:
        if not app.closing:
            app.close()
    return 0
