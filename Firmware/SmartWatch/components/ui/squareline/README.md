# SquareLine Export Directory

Future SquareLine Studio generated files belong in this directory.

Rules:

- Generated files are never edited manually.
- Application logic stays in `components/apps`.
- Service calls and navigation are bridged through `ui_events`.
- SquareLine is a view generator, not an architecture layer.

Expected integration shape:

- Generated widget trees live under `components/ui/squareline/`.
- `components/ui/ui_squareline.c` is the stable adapter layer between generated views and app logic.
- SquareLine buttons should be wired through:
  - `ui_squareline_bind_open_app()`
  - `ui_squareline_bind_back()`
- Apps may request a screen through `ui_squareline_create_screen()` and then populate fallback LVGL content when generated views are not present yet.
