import { Injectable } from '@nestjs/common';
import { ExtensionRepository } from '@/domain/repositories/extension.interface';

import { cpSync, existsSync, mkdirSync, readdirSync } from 'fs';
import { join } from 'path';
import { HttpService } from '@nestjs/axios';
import decompress from 'decompress';
import { exit } from 'node:process';
import { InitOptions } from '@commanders/types/init-options.interface';
import { firstValueFrom } from 'rxjs';
import { FileManagerService } from '@/infrastructure/services/file-manager/file-manager.service';
import { rm } from 'fs/promises';
import {
  CORTEX_ENGINE_RELEASES_URL,
  CUDA_DOWNLOAD_URL,
} from '@/infrastructure/constants/cortex';
import { checkNvidiaGPUExist } from '@/utils/cuda';

import { cpuInfo } from 'cpu-instructions';
import { DownloadManagerService } from '@/infrastructure/services/download-manager/download-manager.service';
import { DownloadType } from '@/domain/models/download.interface';
import { Engines } from '@/infrastructure/commanders/types/engine.interface';

@Injectable()
export class EnginesUsecases {
  constructor(
    private readonly httpService: HttpService,
    private readonly fileManagerService: FileManagerService,
    private readonly downloadManagerService: DownloadManagerService,
    private readonly extensionRepository: ExtensionRepository,
  ) {}

  /**
   * Get the engines
   * @returns Cortex supported Engines
   */
  async getEngines() {
    return (await this.extensionRepository.findAll()).map((engine) => ({
      name: engine.name,
      description: engine.description,
      version: engine.version,
      productName: engine.productName,
      initialized: engine.initalized,
    }));
  }

  /**
   * Get the engine with the given name
   * @param name Engine name
   * @returns Engine
   */
  async getEngine(name: string) {
    return this.extensionRepository.findOne(name).then((engine) =>
      engine
        ? {
            name: engine.name,
            description: engine.description,
            version: engine.version,
            productName: engine.productName,
            initialized: engine.initalized,
          }
        : undefined,
    );
  }

  /**
   * Default installation options base on the system
   * @returns
   */
  defaultInstallationOptions = async (): Promise<InitOptions> => {
    let options: InitOptions = {};

    // Skip check if darwin
    if (process.platform === 'darwin') {
      return options;
    }
    // If Nvidia Driver is installed -> GPU
    options.runMode = (await checkNvidiaGPUExist()) ? 'GPU' : 'CPU';
    options.gpuType = 'Nvidia';
    //CPU Instructions detection
    options.instructions = await this.detectInstructions();
    return options;
  };

  /**
   * Install Engine and Dependencies with given options
   * @param engineFileName
   * @param version
   */
  installEngine = async (
    options?: InitOptions,
    version: string = 'latest',
    engine: string = 'default',
    force: boolean = false,
  ): Promise<any> => {
    // Use default option if not defined
    if (!options && engine === Engines.llamaCPP) {
      options = await this.defaultInstallationOptions();
    }
    // Ship Llama.cpp engine by default
    if (
      !existsSync(
        join(await this.fileManagerService.getCortexCppEnginePath(), engine),
      ) ||
      force
    ) {
      const isVulkan =
        engine === Engines.llamaCPP &&
        (options?.vulkan ||
          (options?.runMode === 'GPU' && options?.gpuType !== 'Nvidia'));
      await this.installAcceleratedEngine(version, engine, [
        process.platform === 'win32'
          ? '-windows'
          : process.platform === 'darwin'
            ? '-mac'
            : '-linux',
        // CPU Instructions - CPU | GPU Non-Vulkan
        options?.instructions && !isVulkan
          ? `-${options?.instructions?.toLowerCase()}`
          : '',
        // Cuda
        options?.runMode === 'GPU' && options?.gpuType === 'Nvidia' && !isVulkan
          ? `cuda-${options.cudaVersion ?? '12'}`
          : '',
        // Vulkan
        isVulkan ? '-vulkan' : '',

        // Arch
        engine !== Engines.tensorrtLLM
          ? process.arch === 'arm64'
            ? '-arm64'
            : '-amd64'
          : '',
      ]);
    }

    if (
      (engine === Engines.llamaCPP || engine === Engines.tensorrtLLM) &&
      options?.runMode === 'GPU' &&
      options?.gpuType === 'Nvidia' &&
      !options?.vulkan
    )
      await this.installCudaToolkitDependency(options?.cudaVersion);

    // Update states
    await this.extensionRepository.findOne(engine).then((e) => {
      if (e) e.initalized = true;
    });
  };

  /**
   * Install CUDA Toolkit dependency (dll/so files)
   * @param options
   */
  private installCudaToolkitDependency = async (cudaVersion?: string) => {
    const platform = process.platform === 'win32' ? 'windows' : 'linux';

    const dataFolderPath = await this.fileManagerService.getDataFolderPath();
    const url = CUDA_DOWNLOAD_URL.replace(
      '<version>',
      cudaVersion === '11' ? '11.7' : '12.3',
    ).replace('<platform>', platform);
    const destination = join(dataFolderPath, 'cuda-toolkit.tar.gz');

    console.log('Downloading CUDA Toolkit dependency...');

    await this.downloadManagerService.submitDownloadRequest(
      url,
      'Cuda Toolkit Dependencies',
      DownloadType.Engine,
      { [url]: destination },
      async () => {
        try {
          await decompress(
            destination,
            await this.fileManagerService.getCortexCppEnginePath(),
          );
        } catch (e) {
          console.log(e);
          exit(1);
        }
        await rm(destination, { force: true });
      },
    );
  };

  private detectInstructions = (): Promise<
    'AVX' | 'AVX2' | 'AVX512' | undefined
  > => {
    const cpuInstruction = cpuInfo.cpuInfo()[0] ?? 'AVX';
    console.log(cpuInstruction, 'CPU instructions detected');
    return Promise.resolve(cpuInstruction);
  };

  /**
   * Download and install accelerated engine
   * @param version
   * @param engineFileName
   */
  private async installAcceleratedEngine(
    version: string = 'latest',
    engine: string = Engines.llamaCPP,
    matchers: string[] = [],
  ) {
    const res = await firstValueFrom(
      this.httpService.get(
        CORTEX_ENGINE_RELEASES_URL(engine) +
          `${version === 'latest' ? '/latest' : ''}`,
        {
          headers: {
            'X-GitHub-Api-Version': '2022-11-28',
            Accept: 'application/vnd.github+json',
          },
        },
      ),
    );

    if (!res?.data) {
      console.log('Failed to fetch releases');
      exit(1);
    }

    let release = res?.data;
    if (Array.isArray(res?.data)) {
      release = Array(res?.data)[0].find(
        (e) => e.name === version.replace('v', ''),
      );
    }
    // Find the asset for the current platform
    const toDownloadAsset = release.assets
      .sort((a: any, b: any) => a.name.length - b.name.length)
      .find((asset: any) =>
        matchers.every((matcher) => asset.name.includes(matcher)),
      );

    if (!toDownloadAsset) {
      console.log(
        `Could not find engine file for platform ${process.platform}`,
      );
      exit(1);
    }

    const engineDir = await this.fileManagerService.getCortexCppEnginePath();

    if (!existsSync(engineDir)) mkdirSync(engineDir, { recursive: true });

    const destination = join(engineDir, toDownloadAsset.name);

    await this.downloadManagerService.submitDownloadRequest(
      toDownloadAsset.browser_download_url,
      engine,
      DownloadType.Engine,
      { [toDownloadAsset.browser_download_url]: destination },
      async () => {
        try {
          await decompress(destination, engineDir);
        } catch (e) {
          console.error('Error decompressing file', e);
          exit(1);
        }
        await rm(destination, { force: true });

        // Copy the additional files to the cortex-cpp directory
        for (const file of readdirSync(join(engineDir, engine))) {
          if (!file.includes('engine')) {
            await cpSync(join(engineDir, engine, file), join(engineDir, file));
          }
        }
      },
    );
  }
}
