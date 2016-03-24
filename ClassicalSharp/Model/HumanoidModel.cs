﻿using System;
using System.Drawing;
using ClassicalSharp.GraphicsAPI;
using OpenTK;

namespace ClassicalSharp.Model {
	
	public class HumanoidModel : IModel {
		
		ModelSet Set, SetSlim, Set64;
		public HumanoidModel( Game window ) : base( window ) {
			vertices = new ModelVertex[boxVertices * ( 7 + 11 )];
			Set = new ModelSet();
			
			BoxDesc head = MakeBoxBounds( -4, 24, -4, 4, 32, 4 ).RotOrigin( 0, 24, 0 );
			BoxDesc torso = MakeBoxBounds( -4, 12, -2, 4, 24, 2 );
			BoxDesc lLeg = MakeBoxBounds( -4, 0, -2, 0, 12, 2 ).RotOrigin( 0, 12, 0 );
			BoxDesc rLeg = MakeBoxBounds( 0, 0, -2, 4, 12, 2 ).RotOrigin( 0, 12, 0 );
			BoxDesc lArm = MakeBoxBounds( -8, 12, -2, -4, 24, 2 ).RotOrigin( -5, 22, 0 );
			BoxDesc rArm = MakeBoxBounds( 4, 12, -2, 8, 24, 2 ).RotOrigin( 5, 22, 0 );
			
			Set.Head = BuildBox( head.TexOrigin( 0, 0 ) );
			Set.Torso = BuildBox( torso.TexOrigin( 16, 16 ) );
			Set.LeftLeg = BuildBox( lLeg.MirrorX().TexOrigin( 0, 16 ) );
			Set.RightLeg = BuildBox( rLeg.TexOrigin( 0, 16 ) );
			Set.Hat = BuildBox( head.TexOrigin( 32, 0 ).Expand( 0.5f ) );
			Set.LeftArm = BuildBox( lArm.TexOrigin( 40, 16 ) );
			Set.RightArm = BuildBox( rArm.TexOrigin( 40, 16 ) );
			
			Set64 = new ModelSet();
			Set64.Head = Set.Head;
			Set64.Torso = Set.Torso;
			Set64.LeftLeg = BuildBox( lLeg.TexOrigin( 16, 48 ) );
			Set64.RightLeg = Set.RightLeg;
			Set64.Hat = Set.Hat;
			Set64.LeftArm = BuildBox( lLeg.TexOrigin( 32, 48 ) );
			Set64.RightArm = Set.RightArm;
			Set64.TorsoLayer = BuildBox( torso.TexOrigin( 16, 32 ).Expand( 0.5f ) );
			Set64.LeftLegLayer = BuildBox( lLeg.TexOrigin( 0, 48 ).Expand( 0.5f ) );
			Set64.RightLegLayer = BuildBox( rLeg.TexOrigin( 0, 32 ).Expand( 0.5f ) );
			Set64.LeftArmLayer = BuildBox( lLeg.TexOrigin( 48, 48 ).Expand( 0.5f ) );
			Set64.RightArmLayer = BuildBox( rLeg.TexOrigin( 40, 32 ).Expand( 0.5f ) );			
			
			SetSlim = new ModelSet();
			SetSlim.Head = Set.Head;
			SetSlim.Torso = Set.Torso;
			SetSlim.LeftLeg = Set64.LeftLeg;
			SetSlim.RightLeg = Set.RightLeg;
			SetSlim.Hat = Set.Hat;
			lArm.BodyW -= 1; lArm.X1 += 1/16f;
			SetSlim.LeftArm = BuildBox( lArm.TexOrigin( 32, 48 ) );
			rArm.BodyW -= 1; rArm.X2 -= 1/16f;
			SetSlim.RightArm = BuildBox( rArm.TexOrigin( 40, 16 ) );
			SetSlim.TorsoLayer = Set64.TorsoLayer;
			SetSlim.LeftLegLayer = Set64.LeftLegLayer;
			SetSlim.RightLegLayer = Set64.RightLegLayer;
			SetSlim.LeftArmLayer = BuildBox( lArm.Expand( 0.5f ) );
			SetSlim.RightArmLayer = BuildBox( rArm.Expand( 0.5f ) );			
		}
		
		public override bool Bobbing { get { return true; } }
		
		public override float NameYOffset { get { return 2.1375f; } }
		
		public override float GetEyeY( Entity entity ) { return 26/16f; }
		
		public override Vector3 CollisionSize {
			get { return new Vector3( 8/16f + 0.6f/16f, 28.1f/16f, 8/16f + 0.6f/16f ); }
		}
		
		public override BoundingBox PickingBounds {
			get { return new BoundingBox( -8/16f, 0, -4/16f, 8/16f, 32/16f, 4/16f ); }
		}
		
		protected override void DrawModel( Player p ) {
			int texId = p.PlayerTextureId <= 0 ? cache.HumanoidTexId : p.PlayerTextureId;
			graphics.BindTexture( texId );
			graphics.AlphaTest = false;
			
			SkinType skinType = p.SkinType;
			_64x64 = skinType != SkinType.Type64x32;
			ModelSet model = skinType == SkinType.Type64x64Slim ? SetSlim :
				(skinType == SkinType.Type64x64 ? Set64 : Set);
			DrawHeadRotate( -p.PitchRadians, 0, 0, model.Head );
			DrawPart( model.Torso );
			
			DrawRotate( p.anim.legXRot, 0, 0, model.LeftLeg );
			DrawRotate( -p.anim.legXRot, 0, 0, model.RightLeg );
			Rotate = RotateOrder.XZY;
			DrawRotate( p.anim.leftXRot, 0, p.anim.leftZRot, model.LeftArm );
			DrawRotate( p.anim.rightXRot, 0, p.anim.rightZRot, model.RightArm );
			Rotate = RotateOrder.ZYX;
			graphics.UpdateDynamicIndexedVb( DrawMode.Triangles, cache.vb, cache.vertices, index, index * 6 / 4 );
			
			graphics.AlphaTest = true;
			index = 0;
			if( _64x64 ) {
				DrawPart( model.TorsoLayer );
				DrawRotate( p.anim.legXRot, 0, 0, model.LeftLegLayer );
				DrawRotate( -p.anim.legXRot, 0, 0, model.RightLegLayer );
				Rotate = RotateOrder.XZY;
				DrawRotate( p.anim.leftXRot, 0, p.anim.leftZRot, model.LeftArmLayer );
				DrawRotate( p.anim.rightXRot, 0, p.anim.rightZRot, model.RightArmLayer );
				Rotate = RotateOrder.ZYX;
			}
			DrawHeadRotate( -p.PitchRadians, 0, 0, model.Hat );
			graphics.UpdateDynamicIndexedVb( DrawMode.Triangles, cache.vb, cache.vertices, index, index * 6 / 4 );
		}
		
		class ModelSet {
			public ModelPart Head, Torso, LeftLeg, RightLeg, LeftArm, RightArm, Hat,
			TorsoLayer, LeftLegLayer, RightLegLayer, LeftArmLayer, RightArmLayer;
		}
	}
}