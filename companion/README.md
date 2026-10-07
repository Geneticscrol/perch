# PERCH companion

Web Serial census viewer. Chrome or Edge. The page only reads.

**GitHub Pages:** [geneticscrol.github.io/perch/companion/](https://geneticscrol.github.io/perch/companion/) after the Pages workflow has run once. If the link 404s, open the repo **Settings → Pages** and set the source to **GitHub Actions**.

Local:

```bash
cd companion
python -m http.server 8080
```

Open `http://localhost:8080`, click Connect, pick the ShrikeFi port at 115200.
