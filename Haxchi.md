# Haxchi Solutions

AM64DS and Haxchi/CBHC cannot be installed to the same title as each other. If you have Haxchi or CBHC installed to your installation of SM64DS, then you will need to do some work to get a compatible installation of SM64DS to use with AM64DS. Below are a number of options you can use to get AM64DS working on your system.

**IF YOU ARE USING CBHC: Before starting,** you should (temporarily) disable coldboot to ensure that you don't accidentally brick your Wii U. There are things that can go wrong when following any of these solutions, so it's best to disable coldboot before following any of these steps, and then re-enabling it only once you've tested that everything is working correctly after a reboot. Follow [these instructions](https://wiiu.hacks.guide/#/uninstall-cbhc) for disabling coldboot.

## Switch to Tiramisu

This is the most straightforward option for resolving this conflict. Tiramisu is an alternative entry point from Haxchi that provides improved safety and functionality. Tiramisu can be setup to work from coldboot like CBHC, but is installed to a system title, reducing the possibility of accidentally bricking the system. Follow [the instructions on wiiu.hacks.guide](https://wiiu.hacks.guide/#/) to change your installation to use Tiramisu. From there, you can simply uninstall the Haxchi-patched game and redownload it to get the game working with the AM64DS patcher.

## Switch to Mocha

Alternatively, you can instead use Mocha instead of Haxchi. This method is not recommended over Tiramisu, but it is an available option. Installing [Mocha + Indexiine](https://wiiu.hacks.guide/#/archive/mocha/indexiine/sd-preparation) along with using a [Configurable Payload configured to launch Mocha](https://wiiu.hacks.guide/#/configurable-payload) is equivalent to a non-coldboot Haxchi installation, and should be convenient enough for most people. Once you've setup Mocha, you can uninstall the Haxchi patched game and redownload it to get the game working with the AM64DS patcher.

## Switch Haxchi/CBHC to a different DS game

If you have another DS game or are willing to buy one, you can install Haxchi/CBHC to a different DS game. The list of compatible games [is available here](https://wiiu.hacks.guide/#/haxchi/ds-vc-choice). Once you have Haxchi/CBHC installed to a new game, you can uninstall the Haxchi patched game and redownload it to get the game working with the AM64DS patcher.

## Install another region's release of SM64DS

There are three eShop releases of SM64DS on the Wii U: North American, European, and Japanese. If you are using one of these regional releases for Haxchi/CBHC, you can install a different regional release and apply the AM64DS Patch to it. Both the North American and European releases work in English, and the regional differences are minor enough that a casual player most likely won't notice them.

## Use a ROM injector tool to create a second install of SM64DS

Most ROM Injection tools that produce an installable WUP file will generate random Title IDs, including Phacox's Injector and UWUVCI. The changed Title ID means that it can be installed along side the Haxchi/CBHC installation without any conflicts. Use SM64DS as the base game and select any SM64DS ROM to inject (even a version not available in the eShop, such as a rev0 or Korean release).

## Make a patched WUP package with a new Title ID

This is a more direct method of getting an SM64DS installation with a different Title ID, but requires more manual work. You can create an installable copy of SM64DS with a different Title ID by taking the original eShop release's WUP package (eg. from NUS Downloader), decrypt it with a tool like Cdecrypt, edit the `meta/meta.xml` file to change the `<title_id>` entry, and encrypt it with a tool like NUS Packer. You need to make sure that the Title ID you use is not used by any games you have installed or are likely to install. One way to do that is to use a randomized demo Title ID of `00050002XXXXXXXX`, replacing the X's with random hex digits (0-9 and A-F).

The edited meta.xml entry should look something like this: `<title_id type="hexBinary" length="8">00050002ABCDEF89</title_id>`
