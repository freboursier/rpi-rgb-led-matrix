 sudo ./dmd-slideshow --led-show-refresh  --led-chain=8 --led-pixel-mapper="U-mapper"  --led-cols=64 --led-slowdown-gpio=3 --led-no-drop-privs  -d ~/gif -c '.*sonic.*' -f '.*bubble.*'

# En cours

 * Transformer Sequence en classe
 * ne pas pré-loader trop d'images si on a une seule Collection dans une séquence
 * Gérer le changement de séquence
 * afficher un splash message au changement de séquence (de préférence pas à la place de l'animation)
 * permettre le changement de collection à la remote