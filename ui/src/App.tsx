export default function App() {
  return (
    <main className="app">
      <header className="app__header">
        <p className="app__eyebrow">Audio workspace</p>
        <h1 className="app__title" data-testid="app-title">
          Instrument Hub
        </h1>
        <p className="app__lede">
          One place for your instruments, controller and set.
        </p>
      </header>

      <section className="app__slots" aria-label="Instrument slots">
        {[1, 2, 3, 4, 5, 6, 7, 8].map((slot) => (
          <article className="slot" key={slot}>
            <span className="slot__number">{slot.toString().padStart(2, "0")}</span>
            <span className="slot__name">Empty slot</span>
          </article>
        ))}
      </section>

      <footer className="app__footer">
        <span className="app__status" data-testid="app-status">
          Engine skeleton
        </span>
        <span className="app__version">v0.1.0</span>
      </footer>
    </main>
  );
}
