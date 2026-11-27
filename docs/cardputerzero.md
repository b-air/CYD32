# CYD32 sur le CardputerZero

Installation depuis une pré-release qui contient `cyd32_0.3.1_arm64.deb` et `SHA256SUMS`. Remplacez `ETIQUETTE` par le nom de cette pré-release. Une commande à la fois.

```sh
curl --proto '=https' --proto-redir '=https' --tlsv1.2 -fL -O https://github.com/b-air/CYD32/releases/download/ETIQUETTE/cyd32_0.3.1_arm64.deb -O https://github.com/b-air/CYD32/releases/download/ETIQUETTE/SHA256SUMS
sha256sum -c SHA256SUMS
sudo apt install ./cyd32_0.3.1_arm64.deb
```

`SHA256SUMS` ne liste que ce `.deb`. L’icône apparaît dans le carrousel. `Q`, ou `Esc` sur l’écran SONAR, y revient. La première fois, une nouvelle session (ou un redémarrage) est nécessaire pour que le compte `pi` soit dans le groupe `cyd32` et puisse lancer le helper radio.

Essai hors carrousel, sans arrêter LightDM et sans tuer le lanceur :

```sh
systemctl --user stop APPLaunch
/usr/share/APPLaunch/bin/cyd32-ui --self-test; echo "code=$?"
systemctl --user start APPLaunch
```

Mise à jour (télécharge le `.deb` le plus récent, vérifie `SHA256SUMS`, installe, affiche la version) :

```sh
cyd32-update
```

Retrait :

```sh
sudo apt remove cyd32
sudo apt purge cyd32
```

`purge` supprime aussi l’utilisateur système `cyd32`. `remove` le laisse.
