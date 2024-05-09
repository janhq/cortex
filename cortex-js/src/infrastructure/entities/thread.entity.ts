import {
  Thread,
  ThreadAssistantInfo,
  ThreadMetadata,
} from '@/domain/models/thread.interface';
import { Entity, PrimaryColumn, Column } from 'typeorm';

@Entity('threads')
export class ThreadEntity implements Thread {
  @PrimaryColumn()
  id: string;

  @Column()
  object: string;

  @Column({ name: 'title' })
  title: string;

  @Column({ type: 'simple-array' })
  assistants: ThreadAssistantInfo[];

  @Column()
  createdAt: number;

  @Column({ nullable: true })
  updatedAt?: number;

  @Column({ nullable: true, type: 'simple-json' })
  metadata?: ThreadMetadata;
}
